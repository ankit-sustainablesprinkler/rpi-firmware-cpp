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
#include <cstdio>

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

#define SERIAL_PORT "/dev/ttyAMA0"
#define WDT_INTERRUPT_PERIOD 30

#define PULSE_4HZ 1
#define PULSE_2HZ 2
#define PULSE_1HZ 4
#define PULSE_OFF 0

using namespace std;
using namespace bin_protocol;
bool runSchedule(run_state_t &state, const Schedule &schedule, const Config &config);
static Modem::ErrType modemPut(Modem &modem, modem_reply_t &message);
void switch_init();
void fault_reset();
void modemThread(bool RTC_fitted);
void WDTThread();
void LEDThread();

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
volatile int led_duration;


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
bool schedule_ready = false, config_ready = false, flow_config_ready = false, firstBoot = true, feedback_ready = false, 
     flow_feedback_ready = false, heartbeat_sent = false, perform_calibration = false, post_run_feedback_ready = false, post_run_flow_feedback_ready = false;

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
	int pid_file = open(PID_FILE, O_CREAT | O_RDWR, 666);
	//try to lock the pid file	
	int rc = flock(pid_file, LOCK_EX | LOCK_NB); //Linux kernel will allways remove this lock if the process exits for some unknown reason.
	if(rc && EWOULDBLOCK == errno) {
		// another instance is running
		cout << "Another instance is currently running. Stop it first before restarting." << endl;
	} else {
		// this is the first instance

		remove(OTA_SYNC_FILE);
		wiringPiSetup();
		switch_init();
		pinMode(PIN_WDT_RESET, OUTPUT);
		pinMode(PIN_STATUS, OUTPUT);
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
		s3state.flow_feedback[0].header.type = bin_protocol::FLOW;
		s3state.flow_feedback[1].header.type = bin_protocol::FLOW;

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
		thread led_thread(LEDThread);

		//load config and schedule
		Schedule schedule;
		Config config;
		FlowConfiguration flow_config;
		CalibrationSetup calibration;

		CalibrationResult cal_result;
		bool has_schedule = getSchedule(schedule);
		cout<< "programs: " << schedule.zone_duration.size() << endl;
		bool has_config = getConfig(config);
		bool has_flow_config = getFlowConfig(flow_config);
		time_t heartbeat_period = HEARTBEAT_MIN_PERIOD;//config.heartbeat_period;
		if(heartbeat_period < HEARTBEAT_MIN_PERIOD) heartbeat_period = HEARTBEAT_MIN_PERIOD;

		if(has_schedule && s3state.feedback[s3state.var.current_feedback].zone_runs.size() == 0){
			s3state.feedback[s3state.var.current_feedback].manual_runs.clear();
			s3state.feedback[s3state.var.current_feedback].zone_runs.resize(schedule.zone_duration.size());
			for(int i = 0; i < schedule.zone_duration.size(); i++){
				s3state.feedback[s3state.var.current_feedback].zone_runs[i].resize(schedule.zone_duration[i].size());
			}
		}

		if(!has_schedule){
				cout << "Allowing OTA" << endl;
				ofstream ota_file(OTA_SYNC_FILE);
				if(ota_file.is_open()){
					ota_file.close();
				}
		}

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


		run_state_t state;
		state.type = NONE;

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
			if(flow_config_ready){
				modem_IO_mutex.lock();
				try{
					getFlowConfig(flow_config);
					flow_config_ready = false;
					cout << "NEW FLOW CONFIG " << flow_config.ID << endl;
				}catch(...){}
				modem_IO_mutex.unlock();
			}
			modem_update_mutex.unlock();



			sensor::sensorRead(state, s3state, schedule, config, flow_config);

			if(!perform_calibration){
				if(schedule.isValid()) runSchedule(state, schedule, config);
			} else {
				static int calibration_state = 0;
				static int prev_calibration_state = -1;
				static int zone_idx = 0;
				time_t ref_time;
				if(calibration_state != prev_calibration_state){
					switch(calibration_state){
						case 0: 
							cout << "Calibration Ready" << endl;
							led_duration = PULSE_OFF;
							break;
						case 1: 
							cout << "Waiting for Idle System" << endl;
							led_duration = PULSE_1HZ;
							break;
						case 2:
							led_duration = PULSE_1HZ;
							cout << "Initializing Calibration for zone " << (int) calibration.zones[zone_idx] << endl;
							break;
						case 3:
							led_duration = PULSE_OFF;
							cout << "Waiting for Zone to run" << endl;
							break;
						case 4:
							led_duration = PULSE_2HZ;
							cout << "Waiting for Flow to stabilize" << endl;
							break;
						case 5:
							cout << "Waiting for Average" << endl;
							led_duration = PULSE_2HZ;
							break;
						case 6:
							cout << "Restarting Zone index " << (int) calibration.zones[zone_idx] << endl;
							led_duration = PULSE_4HZ;
							break;
						case 7:
							led_duration = PULSE_OFF;
							cout << "Finished Calibration" << endl;
							break;
					}
				}
				prev_calibration_state = calibration_state;
				switch(calibration_state){
					case 0:{
						if(getCalibration(calibration)) calibration_state = 1;
						zone_idx = 0;
						cal_result.flow_values.clear();
						s3state.var.calibration_complete = false;

						break;
					}
					case 1:{
						if(zone_idx >= calibration.zones.size()){
							calibration_state = 7;
							break;
						}
						if(!(sensor::voltage_state || sensor::current_state)) calibration_state = 2;
						break;
					}
					case 2:{ //initialization
						closeRelay();
						calibration_state = 3;
					}
					case 3:{
						if(sensor::current_state){
							ref_time = time(NULL);
							calibration_state = 4;
						}
						break;
					}
					case 4:{
						if(!sensor::current_state){
							calibration_state = 6; //turning off the clock will cause the current zone to recalibrate
							ref_time = time(NULL);
							break;
						}
						if(time(NULL) >= ref_time + 10){
							ref_time = time(NULL);
							sensor::flow_average.reset();
							calibration_state = 5;
						}
						break;
					}
					case 5:{
						if(!sensor::current_state){
							calibration_state = 6; //turning off the clock will cause the current zone to recalibrate
							ref_time = time(NULL);
						}
						cout << sensor::flow_average.getSize() << endl;
						if(sensor::flow_average.getSize() >= 60){
							//Record flow
							cal_result.flow_values.push_back(make_tuple<int,float,float>(calibration.zones[zone_idx], sensor::flow_average.getAverage(),
									sensor::flow_average.computeStdDev()));
							bool exists = false;
							for(int i =0; i < s3state.cal_result.flow_values.size(); i++){
								if(calibration.zones[zone_idx] == std::get<0>(s3state.cal_result.flow_values[i])){
									std::get<1>(s3state.cal_result.flow_values[i]) = sensor::flow_average.getAverage();
									std::get<2>(s3state.cal_result.flow_values[i]) = sensor::flow_average.computeStdDev();
									exists = true;
									break;
								}
							}

							if(!exists){
								s3state.cal_result.flow_values.push_back(make_tuple<int,float,float>(calibration.zones[zone_idx], sensor::flow_average.getAverage(),
									sensor::flow_average.computeStdDev()));
							}
							
							zone_idx++;
							openRelay();
							calibration_state = 1;
						}
						break;
					}
					case 6:{
						
						if(time(NULL) >= ref_time + 20){
							calibration_state = 1;
						}
						break;
					}
					case 7:{
						calibration_state = 0;
						perform_calibration = false;
						s3state.var.calibration_complete = true;
						break;
					}
				}
			}

			if(config.heartbeat_period < HEARTBEAT_MIN_PERIOD) config.heartbeat_period = HEARTBEAT_MIN_PERIOD;
			if(time(nullptr) - s3state.var.last_heartbeat_time > config.heartbeat_period){
				if(cal_result.flow_values.size() > 0){
					auto header = getHeader(bin_protocol::FLOW_RESULT);
					cal_result.header = header;
					message_string = cal_result.toBinary();
					cal_result.flow_values.clear();
				} else if(s3state.alert_feedback.alerts.size() > 0){
					auto header = getHeader(bin_protocol::ALERT);
					s3state.alert_feedback.header = header;
					message_string = s3state.alert_feedback.toBinary();
					s3state.alert_feedback.alerts.clear();
				} else if(feedback_ready){
					auto header = getHeader(bin_protocol::FEEDBACK);
					header.timestamp = s3state.var.previous_feedback_time;
					s3state.feedback[!s3state.var.current_feedback].header = header;
					message_string = s3state.feedback[!s3state.var.current_feedback].toBinary();
					//base64_encode(s3state.feedback[!s3state.var.current_feedback].toBinary(), message_string);
				} else if(post_run_feedback_ready){
					auto header = getHeader(bin_protocol::FEEDBACK);
					header.timestamp = s3state.var.current_feedback_time;
					s3state.feedback[s3state.var.current_feedback].header = header;
					message_string = s3state.feedback[s3state.var.current_feedback].toBinary();
					//base64_encode(s3state.feedback[!s3state.var.current_feedback].toBinary(), message_string);
				} else if(flow_feedback_ready){
					auto header = getHeader(bin_protocol::FLOW);
					header.timestamp = s3state.var.previous_flow_feedback_time;
					s3state.flow_feedback[!s3state.var.current_flow_feedback].header = header;
					message_string = s3state.flow_feedback[!s3state.var.current_flow_feedback].toBinary();
				} else if(post_run_flow_feedback_ready){
					auto header = getHeader(bin_protocol::FLOW);
					header.timestamp = s3state.var.current_flow_feedback_time;
					s3state.flow_feedback[s3state.var.current_flow_feedback].header = header;
					message_string = s3state.flow_feedback[s3state.var.current_flow_feedback].toBinary();
				} else {
					
					Heartbeat heartbeat = getHeartbeat(extra_content);
					message_string = heartbeat.toBinary();
					//base64_encode(heartbeat.toBinary(), message_string);
				}
				modem_cond.notify_all();
			}

			state_saveState(s3state);

			//cout << flow_feedback_ready << "  " << feedback_ready << endl;

			this_thread::sleep_for(chrono::seconds(1));
		}
	}
	return 0;
}

bool runSchedule(run_state_t &state, const Schedule &schedule, const Config &config)
{
	time_t now = time(nullptr),midnight;
	midnight = schedule_getMidnight(schedule, config, now);
	//cout << "Midnight: " << midnight << endl;
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

	if(!s3state.flow_feedback[s3state.var.current_flow_feedback].header.timestamp){
		s3state.flow_feedback[s3state.var.current_flow_feedback].header.timestamp = midnight;
	}
	if(!s3state.feedback[s3state.var.current_feedback].header.timestamp){
		s3state.feedback[s3state.var.current_feedback].header.timestamp = midnight;
	}
	
	//check state against schedule to see if relay needs to open or close
	bool closed = getRelayState();
	bool watering_needed = isWateringNeeded(state, schedule, config, now, midnight);

	//dissable irrigation if any fault flags are set
	if(s3state.var.blocked_pump_detected){
		watering_needed = false;
	}

	//Set relay state
	if(watering_needed)
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

	//Parse feedback

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
	//cout << "WHAT?? " << s3state.flow_feedback[s3state.var.current_flow_feedback].samples.size() << endl;
	if(s3state.var.current_flow_feedback_time != midnight) //roll s3state.flow_feedback to next day
	{
		cout << "New s3state.flow_feedback log" << endl;
		if(s3state.var.current_flow_feedback_time){
			cout << "Flow Feedback ready" << endl;
			s3state.var.current_flow_feedback = !s3state.var.current_flow_feedback; // alternate s3state.feedback logs
			flow_feedback_ready = true;
		}
		s3state.var.previous_flow_feedback_time = s3state.var.current_flow_feedback_time;
		s3state.var.current_flow_feedback_time = midnight;
		s3state.flow_feedback[s3state.var.current_flow_feedback].samples.clear();
		s3state.flow_feedback[s3state.var.current_flow_feedback].header.timestamp = midnight;
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
			cout << "Manual run detected" << endl;
			s3state.feedback[s3state.var.current_feedback].manual_runs.push_back(make_tuple<int, uint16_t>(now, 0));
			s3state.alert_feedback.alerts.push_back(make_tuple<int, char, string>(now, 'M', ""));
			cout << s3state.feedback[s3state.var.current_feedback].manual_runs.size();
		}
		s3state.var.current_state_prev = s3state.var.current_state;
	}
	//current off event
	if(!s3state.var.current_state && s3state.var.current_state_prev){
		if(state.type == MANUAL){
			//insert curr_on_time into last event
			if(s3state.feedback[s3state.var.current_feedback].manual_runs.size() > 0){
				cout << "Manual run Ended" << endl;
				get<1>(s3state.feedback[s3state.var.current_feedback].manual_runs.back()) = (sensor::curr_on_time + 30) / 60;
				s3state.alert_feedback.alerts.push_back(make_tuple<int, char, string>(now, 'E', "Duration: " + to_string((sensor::curr_on_time + 30) / 60)));
				sensor::curr_on_time = 0;
			}
		}

		s3state.var.current_state_prev = s3state.var.current_state;
	}

	if(state_changed){
		cout << "State changed from " << s3state.var.previous_state.type << " to " << state.type << endl;
		switch(state.type){
			case BEFORE:
			case AFTER:
			{
				cout << "Allowing OTA" << endl;
				ofstream ota_file(OTA_SYNC_FILE);
				if(ota_file.is_open()){
					ota_file.close();
				}
				post_run_flow_feedback_ready = true;
				post_run_feedback_ready = true;
				break;
			}
			case MANUAL:

				//Clear flags that need to be reset will manual run starts
				s3state.var.blocked_pump_detected = false;
				s3state.var.unscheduled_flow = false;

				break;
			case ZONE:
				//Clear flags that need to reset on zone changes
				s3state.var.low_flow = false;
				s3state.var.high_flow = false;
				s3state.var.very_high_flow = false;
				break;
		}
		switch(s3state.var.previous_state.type){
			case BEFORE:
				s3state.feedback[s3state.var.current_feedback].before_time = sensor::volt_on_time;
				remove(OTA_SYNC_FILE);
				break;
			case AFTER:
				s3state.feedback[s3state.var.current_feedback].after_time = sensor::volt_on_time;
				remove(OTA_SYNC_FILE);
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
						<< ", Xfmr: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].xfmr_voltage=sensor::transformer_voltage_average.getAverage())
						<< ", Current: " <<
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].current=sensor::current_average.getAverage())
						<< ", Flow: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].flow=sensor::flow_average.getAverage())
						<< ", Run time: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].duration=(sensor::curr_on_time+30)/60)
						<< ", Volt Run time: " << 
						(s3state.feedback[s3state.var.current_feedback].zone_runs[s3state.var.previous_state.program][s3state.var.previous_state.zone].volt_duration=(sensor::volt_on_time+30)/60) << endl;
					}
				}
				break;
		}
			//new_sample = handlerObject.getAverage();
		cout << "before reset " << sensor::curr_on_time << endl;
		sensor::resetVoltage();
		sensor::resetCurrent();
		sensor::resetFlow();
		s3state.var.ovc_trigger_count = 0;
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
			//this_thread::sleep_for(chrono::seconds(10));
			if(error == Modem::NONE){
				if(msg_type == bin_protocol::FEEDBACK){
					feedback_ready = false;// reset feedback ready
					post_run_feedback_ready = false;
				}
				if(msg_type == bin_protocol::FLOW){
					flow_feedback_ready = false;// reset flow feedback ready
					post_run_flow_feedback_ready = false;
				}
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
				
				if(type == Type::CONFIG || type == Type::SCHEDULE || type == Type::FLOW_CONFIG || type == Type::FLOW_CAL){
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
						case Type::FLOW_CONFIG:
							cout << "READY for FLOW CONFIG" << endl;
							flow_config_ready = true;
							break;
						case Type::FLOW_CAL:
							cout << "READY for FLOW CALIBRATTION" << endl;
							perform_calibration = true;
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

void LEDThread()
{
	int count = 0;
	while(true){
		if(led_duration){
			if(count < led_duration){
				digitalWrite(PIN_STATUS, HIGH);
			} else {
				digitalWrite(PIN_STATUS, LOW);
			}
			count = (count + 1)%(led_duration*2);
		} else {
			digitalWrite(PIN_STATUS, LOW);
		} 
		this_thread::sleep_for(chrono::milliseconds(125));
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