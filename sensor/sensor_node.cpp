//g++ -std=c++11 -o sensor_node sensor_node.cpp sensor.cpp crc16.cpp moving_average.cpp -lwiringPi -llcm

#include <lcm/lcm-cpp.hpp>
#include <iostream>
#include <fstream>
#include "sensor.h"
#include <unistd.h>
#include "sensor_t.hpp"
#include "sensor_event_t.hpp"
#include <vector>
#include "moving_average.h"


#define FILE_PREFIX "/home/pi/firmware/main/logs/"
#define EVENT_LOG FILE_PREFIX "event_log.txt"
#define SENSOR_LOG FILE_PREFIX "sensor_log.txt"
#define FILTER_DELAY 4
#define CURRENT_THRESHOLD 0.01
#define VOLTAGE_THRESHOLD 10.0
#define FLOW_THRESHOLD 1.0
#define HYSTERESIS 0.05


using namespace std;

bool logEvent(const sensor_event_t &event);
bool logSensor(const sensor_t &sensor);

int main(int argc, char **argv)
{
	lcm::LCM lcm;
	MovingAverage<float> flow_average(4);
	MovingAverage<float> current_average(6);
	MovingAverage<float> voltage_average(6);
	MovingAverage<float> transformer_voltage_average(32);
	sensor_t sensor;
	sensor_event_t sensor_event;
	bool voltage_state = false, voltage_state_prev = false, current_state = false, current_state_prev = false;
	time_t last_log = time(nullptr);
	time_t voltage_start = 0, current_start = 0;

	sensorSetup();
	flowSetSampleSize(10);
	digitalWrite(PIN_FAULT_CLEAR, HIGH);

	do{
		//Read the sensors
		float voltage, solenoid_voltage, solenoid_current;
		meterGetValues(voltage, solenoid_voltage, solenoid_current);
		current_average.addValue(solenoid_current);
		voltage_average.addValue(solenoid_voltage);
		transformer_voltage_average.addValue(voltage);

		float frequency;
		flowGet(frequency);
		flow_average.addValue(frequency);

		//Process the data
		
		time_t now = time(nullptr);
		
		if(now - last_log > 60){
			last_log = now;
			float flow = flow_average.getAverage();
			if(flow > FLOW_THRESHOLD || solenoid_voltage > VOLTAGE_THRESHOLD || solenoid_current > CURRENT_THRESHOLD){
				sensor.voltage = solenoid_voltage;
				sensor.current = solenoid_current;
				sensor.flow = flow;
				cout << "SENSOR " << std::dec << solenoid_voltage << "," << std::dec << solenoid_current << "," << std::dec << flow << endl;
				lcm.publish("SENSOR", &sensor);
				while(!logSensor(sensor));
			}
		}
		
		if(voltage_state){
			if(solenoid_voltage < VOLTAGE_THRESHOLD*(1-HYSTERESIS)){
				voltage_state = false;
			}
		} else {
			if(solenoid_voltage > VOLTAGE_THRESHOLD*(1+HYSTERESIS)){
				voltage_state = true;
				if(!voltage_state_prev) voltage_start = now;
			}
		}
		
		if(current_state){
			if(solenoid_current < CURRENT_THRESHOLD*(1-HYSTERESIS)){
				current_state = false;
			}
		} else {
			if(solenoid_current > CURRENT_THRESHOLD*(1+HYSTERESIS)){
				current_state = true;
				if(!current_state_prev) current_start = now;
			}
		}

		sensor_event.timestamp = now;
		sensor_event.voltage_state = voltage_state;
		sensor_event.current_state = current_state;
		sensor_event.transformer_voltage = transformer_voltage_average.getAverage();
		sensor_event.voltage = voltage_average.getAverage();
		sensor_event.current = current_average.getAverage();
		
		cout << frequency << ", " << sensor_event.transformer_voltage << ", " << sensor_event.voltage << ", " << sensor_event.current << endl;
		
		if(voltage_state && !voltage_state_prev){
			if(now - voltage_start >= FILTER_DELAY){
				voltage_state_prev = voltage_state;
				sensor_event.timestamp = voltage_start;
				cout << "EVENT " << sensor_event.timestamp << "," << voltage_state << "," << current_state << endl;
				lcm.publish("EVENT", &sensor_event);
				while(!logEvent(sensor_event));
			}
		} else if(!voltage_state && voltage_state_prev){
			voltage_state_prev = voltage_state;
			cout << "EVENT " << sensor_event.timestamp << "," << voltage_state << "," << current_state << endl;
			lcm.publish("EVENT", &sensor_event);
			while(!logEvent(sensor_event));
		}

		if(current_state && !current_state_prev){
			if(now - current_start >= FILTER_DELAY){
				current_state_prev = current_state;
				sensor_event.timestamp = current_start;
				cout << "EVENT " << sensor_event.timestamp << "," << voltage_state << "," << current_state << endl;
				lcm.publish("EVENT", &sensor_event);
				while(!logEvent(sensor_event));
			}
		} else if(!current_state && current_state_prev){
			current_state_prev = current_state;
			cout << "EVENT " << sensor_event.timestamp << "," << voltage_state << "," << current_state << endl;
			lcm.publish("EVENT", &sensor_event);
			while(!logEvent(sensor_event));
		}
		
		
		usleep(500000);
	}while(true);
	return 0;
}

bool logEvent(const sensor_event_t &event)
{
	ofstream file;
	file.open(EVENT_LOG, ofstream::out | ofstream::app);
	if(file.is_open()){
		file << event.timestamp << "," << (int)event.voltage_state << "," << (int)event.current_state << "," << event.voltage << "," << event.current << "," << event.transformer_voltage << endl;
		return true;
	} else return false;
}

bool logSensor(const sensor_t &sensor)
{
	ofstream file;
	file.open(SENSOR_LOG, ofstream::out | ofstream::app);
	if(file.is_open()){
		file << sensor.timestamp << "," << sensor.voltage << "," << sensor.current << "," << sensor.flow << endl;
		return true;
	} else return false;
}