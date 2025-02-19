#include "state.h"
#include "base64.h"
#include "bin_protocol.h"

#include <fstream>
#include <iostream>
#include <string>
#include <stdint.h>
#include <vector>

bool state_getState(s3state_t &state)
{
	bool result = false;

	std::ifstream state_file, feedback_file, flow_feedback_file, alert_feedback_file, flow_cal_file;
	state_file.open(STATE_FILE, std::ifstream::binary | std::ifstream::in);
	if(!state_file.is_open()){
		state_file.open(STATE_FILE_OLD, std::ifstream::binary | std::ifstream::in);
		std::cout << "Using old state" << std::endl;
	}
	if(state_file.is_open()){
		long begin, end;
		begin = state_file.tellg();
		state_file.seekg(0, state_file.end);
		end = state_file.tellg();
		if(end - begin == sizeof(state.var)){
			result = true;
			state_file.seekg(0, state_file.beg);
			state_file.read((char*)&state.var, sizeof(state.var));
		} else {
			std::cout << "Invalid size: " << end-begin << std::endl;
		}
	} else {
		std::cout << "No such file: " << STATE_FILE << std::endl;
	}
	
	feedback_file.open(FEEDBACK_FILE, std::ifstream::in);
	if(feedback_file.is_open()){
		std::string b64_string[2];
		if(std::getline(feedback_file, b64_string[0])){
			if(std::getline(feedback_file, b64_string[1])){
				std::vector<uint8_t> data[2];
				if(base64_decode(data[0], b64_string[0]) == BASE64_NO_ERR){
					if(base64_decode(data[1], b64_string[1]) == BASE64_NO_ERR){
						result=true;
						state.feedback[0].fromBinary(data[0]);
						state.feedback[1].fromBinary(data[1]);
					}
				}
			}
		}
	} else {
		std::cout << "No such file: " << FEEDBACK_FILE << std::endl;
	}

	//state.flow_feedback[0].header.type = bin_protocol::FLOW;
	//state.flow_feedback[1].header.type = bin_protocol::FLOW;
	flow_feedback_file.open(FLOW_FEEDBACK_FILE, std::ifstream::in);
	if(flow_feedback_file.is_open()){
		std::string b64_string[2];
		if(std::getline(flow_feedback_file, b64_string[0])){
			if(std::getline(flow_feedback_file, b64_string[1])){
				std::vector<uint8_t> data[2];
				if(base64_decode(data[0], b64_string[0]) == BASE64_NO_ERR){
					if(base64_decode(data[1], b64_string[1]) == BASE64_NO_ERR){
						result=true;
						state.flow_feedback[0].fromBinary(data[0]);
						state.flow_feedback[1].fromBinary(data[1]);
					}
				}
			}
		}
	} else {
		std::cout << "No such file: " << FLOW_FEEDBACK_FILE << std::endl;
	}

	alert_feedback_file.open(ALERT_FEEDBACK_FILE, std::ifstream::in);
	if(alert_feedback_file.is_open()){
		std::string b64_string;
		if(std::getline(flow_feedback_file, b64_string)){
			std::vector<uint8_t> data;
			if(base64_decode(data, b64_string) == BASE64_NO_ERR){
				result=true;
				state.alert_feedback.fromBinary(data);
			}
		}
	} else {
		std::cout << "No such file: " << ALERT_FEEDBACK_FILE << std::endl;
	}

	flow_cal_file.open(FLOW_CAL_RESULTS_FILE, std::ifstream::in);
	if(!flow_cal_file.is_open()){
		flow_cal_file.open(FLOW_CAL_RESULTS_FILE_OLD, std::ifstream::in);
		std::cout << "Using old calibration" << std::endl;
	}
	if(flow_cal_file.is_open()){
		std::string b64_string;
		if(std::getline(flow_cal_file, b64_string)){
			std::vector<uint8_t> data;
			if(base64_decode(data, b64_string) == BASE64_NO_ERR){
				result=true;
				state.cal_result.fromBinary(data);
			}
		}
	} else {
		std::cout << "No such file: " << FLOW_CAL_RESULTS_FILE << std::endl;
	}
	
	//state.flow_feedback[0].header.type = bin_protocol::FLOW;
	//state.flow_feedback[1].header.type = bin_protocol::FLOW;
	return result;
}

bool state_saveState(const s3state_t &state)
{
	bool result = false;
	
	std::ofstream state_file, feedback_file, flow_feedback_file, alert_feedback_file, flow_cal_file;
	state_file.open(STATE_FILE, std::ofstream::binary | std::ifstream::out | std::ifstream::trunc);
	if(state_file.is_open()){
		result = true;
		state_file.write((char*) &state.var, sizeof(state.var));
		state_file.flush();
		state_file.close();
	}
	
	feedback_file.open(FEEDBACK_FILE, std::ifstream::out | std::ifstream::trunc);
	if(feedback_file.is_open()){
		result=true;
		std::string b64_string;
		std::vector<uint8_t> data = state.feedback[0].toBinary();
		base64_encode(data, b64_string);
		feedback_file << b64_string << std::endl;
		data = state.feedback[1].toBinary();
		base64_encode(data, b64_string);
		feedback_file << b64_string;
		feedback_file.flush();
		feedback_file.close();
	}

	flow_feedback_file.open(FLOW_FEEDBACK_FILE, std::ifstream::out | std::ifstream::trunc);
	if(flow_feedback_file.is_open()){
	//	std::cout << state.flow_feedback[0].samples.size() << std::endl;
	//	std::cout << state.flow_feedback[1].samples.size() << std::endl;
		result=true;
		std::string b64_string;
		std::vector<uint8_t> data = state.flow_feedback[0].toBinary();
		base64_encode(data, b64_string);
		flow_feedback_file << b64_string << std::endl;
		data = state.flow_feedback[1].toBinary();
		base64_encode(data, b64_string);
		flow_feedback_file << b64_string;
		flow_feedback_file.flush();
		flow_feedback_file.close();
	}	

	alert_feedback_file.open(ALERT_FEEDBACK_FILE, std::ifstream::out | std::ifstream::trunc);
	if(alert_feedback_file.is_open()){
		result=true;
		std::string b64_string;
		std::vector<uint8_t> data = state.alert_feedback.toBinary();
		base64_encode(data, b64_string);
		alert_feedback_file << b64_string << std::endl;
		alert_feedback_file.flush();
		alert_feedback_file.close();
	}

	flow_cal_file.open(FLOW_CAL_RESULTS_FILE, std::ifstream::out | std::ifstream::trunc);
	if(flow_cal_file.is_open()){
		result=true;
		std::string b64_string;
		std::vector<uint8_t> data = state.cal_result.toBinary();
		base64_encode(data, b64_string);
		flow_cal_file << b64_string << std::endl;
		flow_cal_file.flush();
		flow_cal_file.close();
	}

	return result;
}