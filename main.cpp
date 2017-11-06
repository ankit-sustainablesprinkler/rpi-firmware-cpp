#include <iostream>
#include <fstream>
#include <ctime>
#include <cmath>
#include <sys/file.h>
#include <errno.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <wiringPi.h>
#include <lcm/lcm-cpp.hpp>
#include <queue>

#include "sensor_t.hpp"
#include "base64.h"
#include "functions.h"
#include "schedule.h"
#include "modem.h"
#include "failure_exception.h"
#include "sensor_static.h"
#include "state.h"
#include "bin_protocol.h"
#define PLANCKS_CONSTANT 6.626E-34

#define SERIAL_PORT "/dev/ttyAMA0" //"/dev/ttyAMA0"
#define WDT_INTERRUPT_PERIOD 30

using namespace std;
using namespace bin_protocol;
bool runSchedule(const Schedule &schedule, const Config &config);
static Modem::ErrType modemPut(Modem &modem, modem_reply_t &message);
void switch_init();
void fault_reset();
void modemThread(bool RTC_fitted);
void WDTThread();

mutex modem_mutex, modem_IO_mutex, modem_update_mutex;
condition_variable modem_cond;
/*time_t s3state.var.last_heartbeat_time = 0;
time_t s3state.var.current_feedback_time = 0;
run_state_t s3state.var.previous_state;
Feedback s3state.feedback[2]; // keep two s3state.feedback logs so we can continue logging while waiting for upload.
int s3state.var.current_feedback = 0;*/
std::vector<uint8_t> message_string;
s3state_t s3state;
bool has_modem_info_sent = false;
string extra_content = "";


namespace sensor{
extern time_t volt_on_time, curr_on_time, flow_on_time;
extern bool voltage_state, voltage_state_prev, current_state, current_state_prev;
extern MovingAverage<float> flow_average;
extern MovingAverage<float> current_average;
extern MovingAverage<float> voltage_average;
extern MovingAverage<float> transformer_voltage_average;
}

Modem modem;

Schedule new_schedule;
Config new_Config;
bool schedule_ready = false, config_ready = false, firstBoot = true, feedback_ready = false, heartbeat_sent = false; // Added firstBoot - Anthony

struct sample_t{
	float current;
	float voltage;
	float flow;
};

void logModem(string s);

class Handler 
{
public:
	~Handler() {}
	bool sampling_active;
	float flow_sum;
	float voltage_sum;
	float current_sum;
	int sample_count;
	void reset()
	{
		flow_sum = 0;
		voltage_sum = 0;
		current_sum = 0;
		sample_count = 0;
	}
	
	sample_t getAverage(){
		sample_t sample;
		if(sample_count){
			sample.flow = flow_sum / sample_count;
			sample.voltage = voltage_sum / sample_count;
			sample.current = current_sum / sample_count;
		}
		return sample;
	}

	void handleMessage(const lcm::ReceiveBuffer* rbuf,	const std::string& chan, const sensor_t* msg)
	{
		//cout << "current:  " << msg->current << "  voltage:  " << msg->voltage << "  flow:  " << msg->flow << endl;
		
		if(sampling_active){
			current_sample.flow = msg->flow;
			current_sample.voltage = msg->voltage;
			current_sample.current = msg->current;
			flow_sum+= msg->flow;
			voltage_sum+= msg->voltage;
			current_sum += msg->current;
			sample_count++;
		}
	}
	queue<sample_t> sample_queue;
	sample_t current_sample;
};

Handler handlerObject;


int main(int argc, char **argv)
{
	//try to lock the pid file
	int pid_file = open("/tmp/s3-schedule.pid", O_CREAT | O_RDWR, 666);
	int rc = flock(pid_file, LOCK_EX | LOCK_NB); //Linux kernel will allways remove this lock if the process exits for some unknown reason.
	if(rc && EWOULDBLOCK == errno) {
		// another instance is running
		cout << "Another instance is currently running. Stop it first before restarting." << endl;
	} else {
		// this is the first instance
		wiringPiSetup();
		switch_init();
		pinMode(PIN_WDT_RESET, OUTPUT);
		//pinMode(28, OUTPUT);
		//digitalWrite(28, HIGH);
		digitalWrite(PIN_WDT_RESET, HIGH);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		digitalWrite(PIN_WDT_RESET, LOW);
		sensor::sensorInit();
		
		cout << "Starting" << endl;
		//Initial power on routine
		lcm::LCM lcm;

		if(!lcm.good())
			return 1;

		lcm.subscribe("SENSOR", &Handler::handleMessage, &handlerObject);
		
		state_getState(s3state);

		int modem_fail_count = 0;
		modem.init();
		modem.PowerOff();
		this_thread::sleep_for(chrono::seconds(10));
		modem.PowerOn();
#ifdef DEBUG_MODEM
		cout << "Power on modem" << endl;
		cout << "connecting to modem" << endl;
#endif
		modem.Open(SERIAL_PORT, 115200);

		bool RTC_fitted = true;
		bool triedReset = false;
		/*
			//routine for updating time from RTC
			RTC_fitted = update_from_RTC();
		*/
		thread wdt_thread(WDTThread);
		thread modem_thread(modemThread, RTC_fitted);

		//load config and schedule
		Schedule schedule;
		Config config;
		bool has_schedule = getSchedule(schedule);
		cout<< "programs: " << schedule.zone_duration.size() << endl;
		bool has_config = getConfig(config);
		time_t heartbeat_period = HEARTBEAT_MIN_PERIOD;//config.heartbeat_period;
		if(heartbeat_period < HEARTBEAT_MIN_PERIOD) heartbeat_period = HEARTBEAT_MIN_PERIOD;

		//send first heartbeat
		//string extra_content = "";
		try{
			extra_content = "{\"Phone\":\"" + modem.Phone() + "\",\"IMEI\":\"" + modem.IMEI() + "\"}";
		} catch (exception e) {
			cout << e.what() << endl;
		} catch (...) {

		}
		Heartbeat heartbeat = getHeartbeat(extra_content);
		message_string = heartbeat.toBinary();
		modem_cond.notify_all();
		this_thread::sleep_for(chrono::seconds(1));

		//main loop
		cout << "============ Entering main loop ================" << endl;
		while(true)
		{
			//cout << "Ready: " << feedback_ready << endl;
			//lcm.handleTimeout(50);
			//cout << extra_content <<endl;
			modem_update_mutex.lock();
			if(schedule_ready){
				modem_IO_mutex.lock();
				try{
					getSchedule(schedule);
					schedule_ready = false;
					cout << "NEW SCHEDULE " << schedule.ID << endl;
				}catch(...){}
				modem_IO_mutex.unlock();
			}
			if(config_ready){
				modem_IO_mutex.lock();
				try{
					getConfig(config);
					config_ready = false;
					cout << "NEW CONFIG " << config.ID << endl;
				}catch(...){}
				modem_IO_mutex.unlock();
			}

			modem_update_mutex.unlock();

			sensor::sensorRead();
			if(schedule.isValid()) runSchedule(schedule, config);

			if(config.heartbeat_period < HEARTBEAT_MIN_PERIOD) config.heartbeat_period = HEARTBEAT_MIN_PERIOD;
			if(time(nullptr) - s3state.var.last_heartbeat_time > HEARTBEAT_MIN_PERIOD){//config.heartbeat_period){
				if(feedback_ready){
					auto header = getHeader(bin_protocol::FEEDBACK);
					header.timestamp = s3state.var.previous_feedback_time;
					s3state.feedback[!s3state.var.current_feedback].header = header;
					message_string = s3state.feedback[!s3state.var.current_feedback].toBinary();
					//base64_encode(s3state.feedback[!s3state.var.current_feedback].toBinary(), message_string);
				} else {
					
					Heartbeat heartbeat = getHeartbeat(extra_content);
					message_string = heartbeat.toBinary();
					//base64_encode(heartbeat.toBinary(), message_string);
				}
				modem_cond.notify_all();
			}

			state_saveState(s3state);
			this_thread::sleep_for(chrono::seconds(1));
		}
	}
	return 0;
}

bool runSchedule(const Schedule &schedule, const Config &config)
{
	time_t now = time(nullptr),midnight;
	midnight = schedule_getMidnight(schedule, config, now);
	//cout << "Midnight: " << midnight << endl;
	run_state_t state;
	state.type = NONE;
	
	static int before_time = 0;
	static int after_time = 0;
	static int manual_time = 0;
	
	//get current state
	getRunState(state, schedule, config, now, midnight);

	//check if state changed
	bool state_changed = false;
	if(s3state.var.current_feedback_time){
		if(s3state.var.previous_state.type != state.type){
			state_changed = true;
		} else {
			if(state.type == ZONE){
				if(state.zone != s3state.var.previous_state.zone || state.program != s3state.var.previous_state.program){
					state_changed = true;
				}
			}
		}
	}
	
	//check state against schedule to see if relay needs to open or close
	
	bool closed = getRelayState();
	if(isWateringNeeded(state, schedule, config, now, midnight))
	{
		if(!closed){
			cout << "close" << endl;
			closeRelay();
		}
	} else {
		if(closed){
			cout << "open" << endl;
			openRelay();
		}
	}

	if(s3state.var.current_feedback_time != midnight) //roll s3state.feedback to next day
	{
		cout << "New s3state.feedback log" << endl;
		if(s3state.var.current_feedback_time){
			cout << "Feedback ready" << endl;
			s3state.var.current_feedback = !s3state.var.current_feedback; // alternate s3state.feedback logs
			feedback_ready = true;
		}
		s3state.var.previous_feedback_time = s3state.var.current_feedback_time;
		s3state.var.current_feedback_time = midnight;
		s3state.feedback[s3state.var.current_feedback].manual_runs.clear();
		s3state.feedback[s3state.var.current_feedback].zone_runs.clear();
		s3state.feedback[s3state.var.current_feedback].zone_runs.resize(schedule.zone_duration.size());
		for(int i = 0; i < schedule.zone_duration.size(); i++){
			s3state.feedback[s3state.var.current_feedback].zone_runs[i].resize(schedule.zone_duration[i].size());
		}
	}
	

	//========================== Transition Events =======================
	//Voltage on event
	s3state.var.voltage_state = sensor::voltage_state;
	if(s3state.var.voltage_state && !s3state.var.voltage_state_prev){


		s3state.var.voltage_state_prev = s3state.var.voltage_state;
	}

	//current on event
	s3state.var.current_state = sensor::current_state;
	if(s3state.var.current_state && !s3state.var.current_state_prev){
		if(state.type == MANUAL){
			//insert manual run in feedback
			cout << "Well something is happening..." << endl;
			s3state.feedback[s3state.var.current_feedback].manual_runs.push_back(make_tuple<int, uint16_t>(now, 0));
			cout << s3state.feedback[s3state.var.current_feedback].manual_runs.size();
		}

		s3state.var.current_state_prev = s3state.var.current_state;
	}
	//current off event
	if(!s3state.var.current_state && s3state.var.current_state_prev){
		if(state.type == MANUAL){
			//insert curr_on_time into last event
			if(s3state.feedback[s3state.var.current_feedback].manual_runs.size() > 0){
				cout << "And some other stuff." << endl;
				get<1>(s3state.feedback[s3state.var.current_feedback].manual_runs.back()) = (sensor::curr_on_time + 30) / 60;
				sensor::curr_on_time = 0;
			}
		}

		s3state.var.current_state_prev = s3state.var.current_state;
	}

	if(state_changed){
		cout << "State changed from " << s3state.var.previous_state.type << " to " << state.type << endl;
		switch(s3state.var.previous_state.type){
			case BEFORE:
				s3state.feedback[s3state.var.current_feedback].before_time = sensor::volt_on_time;
				break;
			case AFTER:
				s3state.feedback[s3state.var.current_feedback].after_time = sensor::volt_on_time;
				break;
			case MANUAL:
				cout << "Manual state: " << sensor::curr_on_time << endl;
				//s3state.feedback[s3state.var.current_feedback].manual_time = sensor::curr_on_time;
				break;
			case ZONE:
				cout << "Zone run" << endl;
				handlerObject.sampling_active = false;
				if(s3state.var.previous_state.program < s3state.feedback[s3state.var.current_feedback].zone_runs.size()){
					if(s3state.var.previous_state.zone < s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program].size()){
						cout << "Zone: " << (int)s3state.var.previous_state.zone << ", Pgm: " << (int)s3state.var.previous_state.program << ", Voltage: " <<
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].voltage=sensor::voltage_average.getAverage())
						<< ", Current: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].current=sensor::current_average.getAverage())
						<< ", Flow: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].flow=sensor::flow_average.getAverage())
						<< ", Run time: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].duration=(sensor::curr_on_time+30)/60) << endl;
					}
				}
				break;
		}
			//new_sample = handlerObject.getAverage();
		cout << "before reset " << sensor::curr_on_time << endl;
		sensor::resetVoltage();
		sensor::resetCurrent();
		sensor::resetFlow();
		memcpy(&s3state.var.previous_state, &state, sizeof(state));
	}
//	cout << "before time: " << s3state.feedback[s3state.var.current_feedback].before_time << endl;
	//cout << "after time: " << s3state.feedback[s3state.var.current_feedback].after_time << endl;
//	cout << "manual time: " << s3state.feedback[s3state.var.current_feedback].manual_time << endl;
	return true;
}

static Modem::ErrType modemPut(Modem &modem, modem_reply_t &message)
{
	string requestLine =
		string("PUT ") + "http://" SERVER_HOST SERVER_PATH "/ HTTP/1.1\r\n";

	string headers =
		requestLine + string("Host: ") + SERVER_HOST + "\r\n" +
		"Content-Length: " + to_string(message.request_messageBody.length()) + "\r\n" +
		"Connection: close\r\n" +
		+ "\r\n";
		
	message.port = SERVER_PORT;
	message.host = SERVER_HOST;
	Modem::ErrType error = Modem::UNKNOWN;
	//Modem::ErrType error = modem.HttpRequest(message, SERVER_HOST, SERVER_PORT, requestLine, headers, message.request_messageBody);
	modem.PowerOn();
	this_thread::sleep_for(chrono::seconds(1));
	//modem.init(); // Should this be removed? - Anthony
	modem.Open(SERIAL_PORT, 115200);
	
	if(firstBoot == true){
		modem.waitForReady(); // Modem should be ready... Anthony
		firstBoot = false;
	}
	message.statusCode = 0;

	cout << "Headers: " << headers << endl;
	//message.request_messageBody += "\r\n";
	if(modem.sendRequest(headers, message)){
		if(message.statusCode > 0) error = Modem::NONE;
		else error = Modem::UNKNOWN;
		/*switch(message.statusCode){
			case 200:
			case 302:
				error = Modem::NONE;
				break;
			default:
				error = Modem::UNKNOWN;
		}*/
		//error = message.statusCode == 200 ? Modem::NONE : Modem::UNKNOWN;
	} else {
#if defined DEBUG_MODEM
		cout << "NEED to seppuku" << endl;
#endif
	}
	//modem.PowerOff(); // We want to idle now between heartbeats - Anthony
	
	return error;
}


void modemThread(bool RTC_fitted)
{
	while(true)
	{
		//cout << "treggqrgqg	" << extra_content
		unique_lock<mutex> lk(modem_mutex);
		modem_cond.wait(lk);
		
		int modem_fail_count = 0;
		bool triedReset = false;
		if(extra_content != ""){
			//cout << "CLEARING rtghjetykjhtnjrenwgdghet65y7jhtgraujsyhrtegayusjthyerg" << endl;
			extra_content = "";
			has_modem_info_sent = true;
		}
		
		while(modem_fail_count <= 3){
			
			
			/*Heartbeat heartbeat = getHeartbeat(true);
			
			Feedback s3state.feedback(heartbeat.header);
	
			s3state.feedback.before_time = 560;
			s3state.feedback.after_time =782;
			s3state.feedback.manual_time = 265;
			Schedule schedule;
			getSchedule(schedule);
			s3state.feedback.zone_runs.resize(schedule.zone_duration.size());
			for(int i = 0; i < schedule.zone_duration.size(); i++){
				s3state.feedback.zone_runs[i].resize(schedule.zone_duration[i].size());
				for(int j = 0; j < schedule.zone_duration[i].size(); j++){
					s3state.feedback.zone_runs[i][j].voltage = i*j;
					s3state.feedback.zone_runs[i][j].current = (i+j)*0.1f;
					s3state.feedback.zone_runs[i][j].flow = i+j+20;
					s3state.feedback.zone_runs[i][j].duration = (1+i+j)*5;
					s3state.feedback.zone_runs[i][j].run = (i==1 || j == 1);
				}
			}
			*/
			
			modem_reply_t message;
			base64_encode(message_string, message.request_messageBody);
			auto msg_type = bin_protocol::getTypefromBinary(message_string);
#if defined DEBUG_MODEM
			cout << "REQUEST: " << message.request_messageBody << endl;
#endif
			Modem::ErrType error = Modem::NONE;
			try{
				error = modemPut(modem, message);
				if(!has_modem_info_sent && extra_content == ""){
					try{
						//cout << "YTHJEKJYTJIKJUEUJBTTBVRTYREYTEVYERTYVE" << endl;
						modem_info_t info;
						if(modem.getInfo(info)){
							extra_content = "{\"phone\":" + info.phone + ","\
							"\"imei\":\"" + info.imei + "\","\
							"\"rssi\":" + to_string(info.rssi) + ","\
							"\"manufacturer\":\"" + info.manufacturer + "\","\
							"\"version\":\"" + info.softwareVersion + "\","\
							"\"model\":\"" + info.model + "\"}";
							//cout << "Extra content: " << extra_content << endl;
						} else {
							cout << "Error getting modem params" << endl;
						}
					} catch (exception e) {
						cout << e.what() << endl;
					} catch (...) {
						cout << "What" << endl;
					}
				}
			} catch (FailureException e){
#if defined DEBUG_MODEM
				cout << e.what();
#endif
			} catch (...) { }
			if(error == Modem::NONE){
				if(msg_type == bin_protocol::FEEDBACK) feedback_ready = false;// reset feedback ready
				modem_fail_count = 0;
				int type;
				modem_IO_mutex.lock();
				try{
					setHeartbeatFailCount(0);
#if defined DEBUG_MODEM
					cout << "Tying to handle response" << endl;
#endif
					handleHBResponse(message.response_messageBody, type);
					s3state.var.last_heartbeat_time = time(nullptr);
				} catch(...){}
				modem_IO_mutex.unlock();
				
				if(type == Type::CONFIG || type == Type::SCHEDULE){
					modem_update_mutex.lock();
					switch(type){
						case Type::CONFIG:
							cout << "READY for CONFIG" << endl;
							config_ready = true;
							break;
						case Type::SCHEDULE:
							cout << "READY for SCHEDULE" << endl;
							schedule_ready = true;
							break;
					}
					modem_update_mutex.unlock();
				}
				logModem(to_string(time(nullptr)) + " INFO Modem sent heartbeat");
				break;
			} else {
				logModem(to_string(time(nullptr)) + " ERROR Modem failed to send heartbeat");
				modem_fail_count++;
#if defined DEBUG_MODEM
				cout << "##### Failed to connect." << endl << "Incrementing error count to: " << modem_fail_count << endl;
#endif
				if(modem_fail_count > 3){
					logModem(to_string(time(nullptr)) + " ERROR Modem fail exceeded threshold.");
					modem_IO_mutex.lock();
					try{
						if(incrementHeartbeatFail() > 3 && RTC_fitted){
							modem_fail_count = 0; //we can just continue since RTC is present
						} else { //Just keep commiting seppuku since there is nothing the system can do without the updated time.
							//reboot()
							if(!triedReset){
								triedReset = true;
								logModem(to_string(time(nullptr)) + " INFO Hard reseting modem.");
								modem.hardReset();
								modem.Open(SERIAL_PORT, 115200);
							} else {
								triedReset = false;
								if(RTC_fitted){
#if defined DEBUG_MODEM
									cout << "Commiting seppuku" << endl;
#endif
									logModem(to_string(time(nullptr)) + " WARN Commiting seppuku");
									//modem_fail_count = 0;
									rebootSystem();
									this_thread::sleep_for(chrono::seconds(60));
									//exit(2);
								}
							}
						}
					} catch(...){}
					modem_IO_mutex.unlock();
				}
				logModem(to_string(time(nullptr)) + " INFO Soft reseting modem.");
				//modem.Reset(); // This function does not do anything. read comment under modem.cpp - Anthony
				this_thread::sleep_for(chrono::seconds(30)); // wait before trying again;
			}
		}
	}
}

void WDTThread()
{
	while(true){
		//pulse WDT reset pin
		digitalWrite(PIN_WDT_RESET, HIGH);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		digitalWrite(PIN_WDT_RESET, LOW);
		this_thread::sleep_for(chrono::seconds(10));
	}
}

void switch_init()
{

	pinMode(7, OUTPUT);											/* P1_7:  RESET											*/
	pinMode(4, INPUT);											/* P1_16: ~SYS_FAULT									*/
	pinMode(3, OUTPUT);											/* P1_15: DRIVE											*/

	digitalWrite(7, LOW);
	fault_reset();
}

void fault_reset()
{
	digitalWrite(7, HIGH);
	this_thread::sleep_for(chrono::milliseconds(50));
	digitalWrite(7, LOW);
}

void logModem(string s)
{
	ofstream file;
	file.open(MODEM_LOG, ofstream::out | ofstream::app);
	if(file.is_open()){
		file << s << endl;
		file.close();
	}
}