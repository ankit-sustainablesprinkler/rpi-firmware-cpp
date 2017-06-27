#include "sensor_global.h"

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
namespace Sensor{

static MovingAverage<float> flow_average(4);
static MovingAverage<float> current_average(6);
static MovingAverage<float> voltage_average(6);
static MovingAverage<float> transformer_voltage_average(32);
static sensor_t sensor;
static sensor_event_t sensor_event;

static void init()
{
	sensorSetup();
	flowSetSampleSize(10);
	digitalWrite(PIN_FAULT_CLEAR, HIGH);
}

static void sensorRead()
{
	
	float voltage, solenoid_voltage, solenoid_current;
	meterGetValues(voltage, solenoid_voltage, solenoid_current);
	current_average.addValue(solenoid_current);
	voltage_average.addValue(solenoid_voltage);
	transformer_voltage_average.addValue(voltage);

	float frequency;
	flowGet(frequency);
	flow_average.addValue(frequency);
	
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
}
	
	
	
	
	
	
}