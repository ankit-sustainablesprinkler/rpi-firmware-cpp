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

#define SERIAL_PORT "/dev/ttyAMA0" //"/dev/ttyAMA0"

using namespace std;
using namespace bin_protocol;
bool runSchedule(const Schedule &schedule, const Config &config);
bool runRemote();
static Modem::ErrType modemPut(Modem &modem, modem_reply_t &message);
void switch_init();
void fault_reset();
void modemThread(bool RTC_fitted);

mutex modem_mutex, modem_IO_mutex, modem_update_mutex;
condition_variable modem_cond;
time_t last_heartbeat_time = 0;

Modem modem;

Schedule new_schedule;
Config new_Config;
bool schedule_ready = false, config_ready = false;

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

	void handleMessage(const lcm::ReceiveBuffer* rbuf,	const std::string& chan, const sensor_t* msg)
	{
		cout << "current:  " << msg->current << "  voltage:  " << msg->voltage << "  flow:  " << msg->flow << endl;
		sample_t sample;
		sample.current = msg->current;
		sample.voltage = msg->voltage;
		sample.flow = msg->flow;
		sample_queue.push(sample);
		while(sample_queue.size() > 1024){
			sample_queue.pop();
		}
	}
	queue<sample_t> sample_queue;
};


int main(int argc, char **argv)
{
	//try to lock the pid file
	cout << "Trying to start" << endl;
	int pid_file = open("/tmp/s3-schedule.pid", O_CREAT | O_RDWR, 666);
	int rc = flock(pid_file, LOCK_EX | LOCK_NB); //Linux kernel will allways remove this lock if the process exits for some unknown reason.
	if(rc && EWOULDBLOCK == errno) {
		// another instance is running
	} else {
		// this is the first instance
		
		cout << "Starting" << endl;
		//Initial power on routine
		lcm::LCM lcm;

		if(!lcm.good())
			return 1;

		Handler handlerObject;
		lcm.subscribe("SENSOR", &Handler::handleMessage, &handlerObject);
		wiringPiSetup();
		switch_init();

		int modem_fail_count = 0;
		modem.init();
		modem.PowerOff();
		this_thread::sleep_for(chrono::seconds(10));
		modem.PowerOn();
		cout << "Power on modem" << endl;
		cout << "connecting to modem" << endl;
		modem.Open(SERIAL_PORT, 115200);
		/*
		cout << "Waiting for Modem to boot" << endl;
		while(!modem.waitForReady(120)){
			cout << "modem failed to boot" << endl;
			modem.PowerOff();
			this_thread::sleep_for(chrono::seconds(10));
			modem.PowerOn();
			cout << "Waiting for Modem to boot" << endl;
			modem.Open(SERIAL_PORT, 115200);
		}
		//this_thread::sleep_for(chrono::seconds(5));
		cout << "modem ready" << endl;
		
		modem_info_t info;
		if(modem.getInfo(info)){
			cout << "Phone: " << info.phone << endl;
			cout << "RSSI: " << info.rssi << endl;
			cout << "IMEI: " << info.imei << endl;
			cout << "Manufacturer: " << info.manufacturer << endl;
			cout << "Version: " << info.softwareVersion << endl;
			cout << "Model: " << info.model << endl;
		}*/
		

		bool RTC_fitted = false;
		bool triedReset = false;
		/*
			//routine for updating time from RTC
			RTC_fitted = update_from_RTC();
		*/

		thread modem_thread(modemThread, RTC_fitted);

		//load config and schedule
		Schedule schedule;
		Config config;
		bool has_schedule = getSchedule(schedule);
		bool has_config = getConfig(config);
		time_t heartbeat_period = HEARTBEAT_MIN_PERIOD;//config.heartbeat_period;
		if(heartbeat_period < HEARTBEAT_MIN_PERIOD) heartbeat_period = HEARTBEAT_MIN_PERIOD;
		
		//try and send first heartbeat
		while(true){
			Heartbeat heartbeat = getHeartbeat(true);
			modem_reply_t message; 
			base64_encode(heartbeat.toBinary(), message.request_messageBody);
			Modem::ErrType error = Modem::NONE;
			try{
				error = modemPut(modem, message);
			} catch (FailureException e){
				cout << e.what();
			} catch (...) { }
			if(error == Modem::NONE){
				modem_fail_count = 0;
				setHeartbeatFailCount(0);
				cout << "Tying to handle response" << endl;
				int type;
				handleHBResponse(message.response_messageBody, type);
				last_heartbeat_time = time(nullptr);
				
				logModem(to_string(time(nullptr)) + " INFO Modem sent heartbeat");
				break;
			} else {
				logModem(to_string(time(nullptr)) + " ERROR Modem failed to send heartbeat");
				modem_fail_count++;
				cout << "##### Failed to connect." << endl << "Incrementing error count to: " << modem_fail_count << endl;
				if(modem_fail_count > 3){
					logModem(to_string(time(nullptr)) + " ERROR Modem fail exceeded threshold.");
					if(incrementHeartbeatFail() > 3 && RTC_fitted){
						modem_fail_count = 0; //we can just continue since RTC is present
						break;
					} else { //Just keep commiting seppuku since there is nothing the system can do without the updated time.
						//reboot()
						if(!triedReset){
								logModem(to_string(time(nullptr)) + " INFO Hard reseting modem.");
								triedReset = true;
								modem.hardReset();
								modem.Open(SERIAL_PORT, 115200);
						} else {
							triedReset = false;
							cout << "Commiting seppuku" << endl;
							logModem(to_string(time(nullptr)) + " WARN Commiting seppuku.");
							//modem_fail_count = 0;
							rebootSystem();
							this_thread::sleep_for(chrono::seconds(60));
							//return 2;
						}
					}
				}
				
				logModem(to_string(time(nullptr)) + " INFO Soft reseting modem.");
				modem.Reset();
				this_thread::sleep_for(chrono::seconds(30)); // wait before trying again;
			}
		}
		
		
		//main loop
		cout << "============ Entering main loop ================" << endl;
		while(true)
		{
			//lcm.handleTimeout(50);
			if(config.heartbeat_period < HEARTBEAT_MIN_PERIOD) config.heartbeat_period = HEARTBEAT_MIN_PERIOD;
			if(time(nullptr) - last_heartbeat_time > HEARTBEAT_MIN_PERIOD){//config.heartbeat_period){
				modem_cond.notify_all();
			}
			
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
			
			runSchedule(schedule, config);
			this_thread::sleep_for(chrono::seconds(1));
		}
	}
	return 0;
}

bool runSchedule(const Schedule &schedule, const Config &config)
{
	time_t now = time(nullptr);
	bool closed = getRelayState();
	if(isWateringNeeded(schedule, config, now))
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
	return true;
}


bool runRemote()
{
	static time_t last_heartbeat_time = 0;
	static bool first_heartbeat_sent = false;
	return true;
}

static Modem::ErrType modemPut(Modem &modem, modem_reply_t &message)
{
	string requestLine =
		string("PUT ") + "http://" + SERVER_HOST + "/ HTTP/1.1\r\n";

	string headers =
		requestLine + string("Host: ") + SERVER_HOST + "\r\n" +
		"User-Agent: S3-Burn/0.1 (Linux armv6l; en-US)\r\n" +
		"Content-Length: " + to_string(message.request_messageBody.length()) + "\r\n" +
		"Connection: close\r\n" +
		+ "\r\n";
		
	message.port = SERVER_PORT;
	message.host = SERVER_HOST;
	Modem::ErrType error = Modem::UNKNOWN;
	//Modem::ErrType error = modem.HttpRequest(message, SERVER_HOST, SERVER_PORT, requestLine, headers, message.request_messageBody);
	modem.PowerOn();
	//this_thread::sleep_for(chrono::seconds(1));
//	modem.init();
	modem.Open(SERIAL_PORT, 115200);
	modem.waitForReady();
	if(modem.sendRequest(headers, message)){
		error = message.statusCode == 200 ? Modem::NONE : Modem::UNKNOWN;
	} else {
		cout << "NEED to seppuku" << endl;
	}
	modem.PowerOff();
	
	return error;
}

void modemThread(bool RTC_fitted)
{
	while(true)
	{
		unique_lock<mutex> lk(modem_mutex);
		modem_cond.wait(lk);
		
		int modem_fail_count = 0;
		bool triedReset = false;
		
		while(modem_fail_count <= 3){
			Heartbeat heartbeat = getHeartbeat(true);
			modem_reply_t message; 
			base64_encode(heartbeat.toBinary(), message.request_messageBody);
			Modem::ErrType error = Modem::NONE;
			try{
				error = modemPut(modem, message);
			} catch (FailureException e){
				cout << e.what();
			} catch (...) { }
			if(error == Modem::NONE){
				modem_fail_count = 0;
				int type;
				modem_IO_mutex.lock();
				try{
					setHeartbeatFailCount(0);
					cout << "Tying to handle response" << endl;
					handleHBResponse(message.response_messageBody, type);
					last_heartbeat_time = time(nullptr);
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
				cout << "##### Failed to connect." << endl << "Incrementing error count to: " << modem_fail_count << endl;
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
									cout << "Commiting seppuku" << endl;
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
				modem.Reset();
				this_thread::sleep_for(chrono::seconds(30)); // wait before trying again;
			}
		}
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