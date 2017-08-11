#include "sensor_static.h"

#include <iostream>
#include <fstream>
#include "sensor.h"
#include <unistd.h>
#include <vector>



#define FILE_PREFIX "/home/pi/firmware/main/logs/"
#define EVENT_LOG FILE_PREFIX "event_log.txt"
#define SENSOR_LOG FILE_PREFIX "sensor_log.txt"
#define FILTER_DELAY 4
#define CURRENT_THRESHOLD 0.08
#define VOLTAGE_THRESHOLD 10.0
#define FLOW_THRESHOLD 1.0
#define HYSTERESIS 0.05
namespace sensor{
	
bool voltage_state = false, voltage_state_prev = false, current_state = false, current_state_prev = false;
time_t last_log = time(nullptr);
time_t voltage_start = 0, current_start = 0;
time_t volt_on_time = 0, curr_on_time = 0, flow_on_time = 0;
time_t volt_on_time_prev = 0, curr_on_time_prev = 0, flow_on_time_prev = 0;

MovingAverage<float> flow_average(50);
MovingAverage<float> current_average(50);
MovingAverage<float> voltage_average(50);
MovingAverage<float> transformer_voltage_average(120);


void sensorInit()
{
	sensorSetup();
	flowSetSampleSize(10);
	digitalWrite(PIN_FAULT_CLEAR, HIGH);
}

void sensorRead()
{
	float voltage, solenoid_voltage, solenoid_current, flow;
	meterGetValues(voltage, solenoid_voltage, solenoid_current);
	current_average.addValue(solenoid_current);
	voltage_average.addValue(solenoid_voltage);
	transformer_voltage_average.addValue(voltage);	
	flowGet(flow);
	flow_average.addValue(flow);
	//std::cout << "Voltage: " << solenoid_voltage << ", Current: " << solenoid_current << ", Flow: " << flow << std::endl;
	//std::cout << "Voltage avg: " << voltage_average.getAverage() << ", Current avg: " << current_average.getAverage()
	//<< ", Flow avg: " << flow_average.getAverage() << std::endl;
	
	
	//std::cout << solenoid_voltage << " V, " << solenoid_current << " A" << std::endl;
	
	//voltage = voltage * voltage_count++ + new
	
	time_t now = time(nullptr);
		
	if(now - last_log > 60){
		last_log = now;
		float flow = flow_average.getAverage();
		if(flow > FLOW_THRESHOLD || solenoid_voltage > VOLTAGE_THRESHOLD || solenoid_current > CURRENT_THRESHOLD){
		//	sensor.voltage = solenoid_voltage;
		//	sensor.current = solenoid_current;
		//	sensor.flow = flow;
		//	cout << "SENSOR " << std::dec << solenoid_voltage << "," << std::dec << solenoid_current << "," << std::dec << flow << endl;
		//	lcm.publish("SENSOR", &sensor);
		//	while(!logSensor(sensor));


		}
	}
	
	if(voltage_state){
		if(solenoid_voltage < VOLTAGE_THRESHOLD*(1-HYSTERESIS)){
			voltage_state = false;
			if(now > voltage_start){
				std::cout << "Voltage off" << std::endl;
				//volt_on_time += now - voltage_start;
				std::cout << "Voltage dur: " << volt_on_time << ", Avg: " << voltage_average.getAverage() << std::endl;
			}
		}
	} else {
		if(solenoid_voltage > VOLTAGE_THRESHOLD*(1+HYSTERESIS)){
			voltage_state = true;
			std::cout << "Voltage on" << std::endl;
			//if(!voltage_state_prev) voltage_start = now;
			voltage_start = now;
			volt_on_time_prev = volt_on_time;
		}
	}
	
	if(voltage_state){
		volt_on_time = volt_on_time_prev + now - voltage_start;
	}

	if(current_state){
		if(solenoid_current < CURRENT_THRESHOLD*(1-HYSTERESIS)){
			current_state = false;
			if(now > current_start){
				std::cout << "Current off" << std::endl;
				//curr_on_time += now - current_start;
				std::cout << "Current dur: " << curr_on_time << ", Avg: " << current_average.getAverage() << std::endl;
			}
		}
	} else {
		if(solenoid_current > CURRENT_THRESHOLD*(1+HYSTERESIS)){ 
			current_state = true;
			std::cout << "Current on" << std::endl;
			current_start = now;
			curr_on_time_prev = curr_on_time;
		}
	}

	if(current_state){
		curr_on_time = curr_on_time_prev + now - current_start;
	}


	
	//std::cout << "voltage time: " << volt_on_time << " current time" << curr_on_time << std::endl;
}

void resetCurrent()
{
	current_average.reset();
	curr_on_time = 0;
	current_state = false;
}

void resetVoltage()
{
	voltage_average.reset();
	volt_on_time = 0;
	voltage_state = false;
}

void resetFlow()
{
	flow_average.reset();
}
	
	
	
	
	
	
}