#include "schedule.h"
#include "sensor_static.h"
#include "constants.h"

#include <iostream>
#include <fstream>
#include "sensor.h"
#include <unistd.h>
#include <vector>



#define FILE_PREFIX "/home/pi/firmware/main/logs/"
#define EVENT_LOG FILE_PREFIX "event_log.txt"
#define SENSOR_LOG FILE_PREFIX "sensor_log.txt"
#define FILTER_DELAY 4 //seconds
#define CURRENT_THRESHOLD 0.06 //None of the clocks in the office had a standby current of more than 60 mA
#define VOLTAGE_THRESHOLD 10.0 //abitrary value
#define BLOCKED_PUMP_THRESOLD 60
#define FLOW_THRESHOLD 1.0
#define HYSTERESIS 0.05 //5%
namespace sensor{
	
bool voltage_state = false, current_state = false;
time_t last_log = time(nullptr);
time_t voltage_start = 0, current_start = 0;
time_t volt_on_time = 0, curr_on_time = 0, flow_on_time = 0;
time_t volt_on_time_prev = 0, curr_on_time_prev = 0, flow_on_time_prev = 0;

MovingAverage<float> flow_average(60);
MovingAverage<float> per_minute_flow(60);
MovingAverage<float> current_average(50);
MovingAverage<float> voltage_average(50);
MovingAverage<float> transformer_voltage_average(120);


void sensorInit()
{
	sensorSetup();
	flowSetSampleSize(1);
	digitalWrite(PIN_FAULT_CLEAR, HIGH);
	digitalWrite(PIN_FAULT_CLEAR, LOW);
}

void sensorRead(run_state_t &run_state, s3state_t &state, bin_protocol::Schedule &schedule, bin_protocol::Config &config, bin_protocol::FlowConfiguration &flow_configuration)
{
	float voltage, solenoid_voltage, solenoid_current, flow, flow_raw;
	meterGetValues(voltage, solenoid_voltage, solenoid_current);
	current_average.addValue(solenoid_current);
	voltage_average.addValue(solenoid_voltage);
	transformer_voltage_average.addValue(voltage);
	
	float current_threshold = config.current_on_thr_mA / 1000.0;
	if(current_threshold <= 0.0) current_threshold = CURRENT_THRESHOLD;

	static int last_ovc_trigger = 0;
	static bool ovc_state = false;
	static bool ovc_prev_state = false;
	ovc_state = digitalRead(PIN_SYS_FAULT);

	if(ovc_state){
		if(!ovc_prev_state){
			last_ovc_trigger = time(nullptr);
			if(state.var.ovc_trigger_count < 3){
				state.var.ovc_trigger_count++;
				if(state.var.ovc_trigger_count == 3){
					state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>((int)last_ovc_trigger, 'F',std::string("OVC fault")));
				}
			}
			std::cout << "OVC Fault triggered " << state.var.ovc_trigger_count << std::endl;
		} else if(time(nullptr) - last_ovc_trigger > 10 && state.var.ovc_trigger_count < 3){
			ovc_state = false;
			digitalWrite(PIN_FAULT_CLEAR, HIGH);
			digitalWrite(PIN_FAULT_CLEAR, LOW);
		}
	} else {
		state.var.ovc_trigger_count = 0;
	}
	ovc_prev_state = ovc_state;

	if(config.flow_fitted){
		if(flowGet(flow_raw)){
			if(flow_raw != 0){
				flow = flow_configuration.K*(flow_raw + flow_configuration.offset);
			}
			
			static int blocked_pump_detected_count = 0;
			static int unscheduled_flow_count = 0;
			static int low_flow_count = 0;
			static int high_flow_count = 0;
			static int very_high_flow_count = 0;
			flow_average.addValue(flow);

			per_minute_flow.addValue(flow);
			
			if(config.pump_fitted){
				if(flow < flow_configuration.flow_thr_min && solenoid_current > current_threshold){
					if(!state.var.blocked_pump_detected){
						blocked_pump_detected_count ++;
						if(blocked_pump_detected_count > BLOCKED_PUMP_THRESOLD){
							std::cout << "Blocked pump detected" << std::endl;
							state.var.blocked_pump_time = time(NULL);
							state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>(state.var.blocked_pump_time, 'P',""));
							state.var.blocked_pump_detected = true;
						}
					}
				} else {
					blocked_pump_detected_count = 0;
				}
			}
			if(flow_raw > 0 && solenoid_current < current_threshold){
				if(!state.var.unscheduled_flow){
					unscheduled_flow_count ++;
					if(unscheduled_flow_count > (60 * flow_configuration.leak_count_thresh)){
						std::cout << "Leak detected" << std::endl;
						state.var.unscheduled_flow_time = time(NULL);
						state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>(state.var.unscheduled_flow_time, 'U',"Flow: " + std::to_string(per_minute_flow.getAverage())));
						state.var.unscheduled_flow = true;
					}
				}
			} else {
				unscheduled_flow_count = 0;
				state.var.unscheduled_flow = false;
			}
			if(run_state.type == ZONE){
				bool calibrated = false;
				float threshold = 0;
				for(auto value : state.cal_result.flow_values){
					if(run_state.zone == std::get<0>(value)){
						threshold = std::get<1>(value);
						calibrated = true;
						break;
					}
				}
				if(calibrated && flow < (1-flow_configuration.flow_thr_low/100.0)*threshold && solenoid_current > current_threshold){
					if(!state.var.low_flow){
						low_flow_count ++;
						if(low_flow_count > flow_configuration.flow_count_thr){
							std::cout << "Low flow detected" << std::endl;
							state.var.low_flow_time = time(NULL);
							state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>(state.var.low_flow_time, 'L',"Flow: " + std::to_string(flow)));
							state.var.low_flow = true;
						}
					}
				} else {
					low_flow_count = 0;
				}
				if(calibrated && flow > (1+flow_configuration.flow_thr_low/50.0)*threshold && solenoid_current > current_threshold){
					if(!state.var.very_high_flow){
						very_high_flow_count ++;
						if(very_high_flow_count > flow_configuration.flow_count_thr){
							std::cout << "very high flow detected" << std::endl;
							state.var.very_high_flow_time = time(NULL);
							state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>(state.var.very_high_flow_time, 'V',"Flow: " + std::to_string(flow)));
							state.var.very_high_flow = true;
						}
					}
				} else {
					very_high_flow_count = 0;
				}

				if(calibrated && flow > (1+flow_configuration.flow_thr_low/100.0)*threshold && solenoid_current > current_threshold){
					if(!state.var.high_flow){
						high_flow_count ++;
						if(high_flow_count > flow_configuration.flow_count_thr){
							std::cout << "high flow detected" << std::endl;
							state.var.high_flow_time = time(NULL);
							state.alert_feedback.alerts.push_back(std::make_tuple<int,char,std::string>(state.var.high_flow_time, 'H',"Flow: " + std::to_string(flow)));
							state.var.high_flow = true;
						}
					}
				} else {
					high_flow_count = 0;
				}
			}
		}
	}
	
	time_t now = time(nullptr);
	
	//per minute logging
	if(now - last_log >= 60){
		last_log = now;
		if(flow > 0.01){
			std::cout << "Logging flow " << per_minute_flow.getAverage() << std::endl;
			state.flow_feedback[state.var.current_flow_feedback].samples.push_back(std::make_tuple(now, per_minute_flow.getAverage()));
			std::cout << "Size: " << state.flow_feedback[state.var.current_flow_feedback].samples.size() << std::endl;
		}
		per_minute_flow.reset();
	}
	
	if(voltage_state){
		if(solenoid_voltage < VOLTAGE_THRESHOLD*(1-HYSTERESIS)){
			voltage_state = false;
			if(now > voltage_start){
				std::cout << "Voltage off" << std::endl;
				std::cout << "Voltage dur: " << volt_on_time << ", Avg: " << voltage_average.getAverage() << std::endl;
			}
		}
	} else {
		if(solenoid_voltage > VOLTAGE_THRESHOLD*(1+HYSTERESIS)){
			voltage_state = true;
			std::cout << "Voltage on" << std::endl;
			voltage_start = now;
			volt_on_time_prev = volt_on_time;
		}
	}
	
	if(voltage_state){
		volt_on_time = volt_on_time_prev + now - voltage_start;
	}

	if(current_state){
		if(solenoid_current < current_threshold*(1-HYSTERESIS)){
			current_state = false;
			if(now > current_start){
				std::cout << "Current off" << std::endl;
				std::cout << "Current dur: " << curr_on_time << ", Avg: " << current_average.getAverage() << std::endl;
			}
		}
	} else {
		if(solenoid_current > current_threshold*(1+HYSTERESIS)){ 
			current_state = true;
			std::cout << "Current on" << std::endl;
			current_start = now;
			curr_on_time_prev = curr_on_time;
		}
	}

	if(current_state){
		curr_on_time = curr_on_time_prev + now - current_start;
	}

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