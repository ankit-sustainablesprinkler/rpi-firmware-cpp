#include "functions.h"

#include <wiringPi.h>
#include "modem.h"
#include <sstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <regex>
#include <ctime>
#include <iostream>
#include <fstream>

uint32_t getSerialNumber()
{
	std::ifstream file("/proc/cpuinfo");
	if(file.is_open())
	{
		std::string line;
		while(getline(file, line)){
			if(!line.compare(0,6,"Serial")){
				std::string serial(line.begin()+10, line.begin()+26);
				return std::stoul(serial, nullptr, 16);
			}
		}
	}
	return 0;
}

float getTemperature()
{
	auto output = runCommand("vcgencmd measure_temp");
	std::smatch results;
	std::regex e("-?\\d+\\.\\d*");
	
	if(std::regex_search(output, results, e))
	{
		return std::stof(results[0]);
	}
	return 0.0;
}

uint32_t getUpTime()
{
	std::ifstream file("/proc/uptime");
	if(file.is_open())
	{
		std::string line, data;
		while(getline(file, line)){
			data += line;
		}
		std::smatch results;
		std::regex e("\\d+\\.?\\d*");
		if(std::regex_search(data, results, e))
		{
			return (int)std::stof(results[0]);
		}
	}
	return 0;
}
void setSystemTime(time_t time)
{
	std::cout << "SYS_INFO: Setting date" << std::endl;
	runCommand("date +%s -s @" + std::to_string(time));
	runCommand("hwclock -w");
}

int getHeartbeatFailCount()
{
	int result = 0;
	std::ifstream file(HEARTBEAT_FAIL_COUNTER);
	try{
		file >> std::dec >> result;
	}catch(...){}
	return result;
}

void setHeartbeatFailCount(int count)
{
	std::ofstream file(HEARTBEAT_FAIL_COUNTER);
	try{
		file << std::dec << count;
	} catch(...){}
}

int incrementHeartbeatFail()
{
	int result = getHeartbeatFailCount() + 1;
	setHeartbeatFailCount(result);
	return result;
}

bool getConfig(bin_protocol::Config &config)
{
	
	std::ifstream file(CONFIG_FILE);
	if(file.is_open()){
		
		std::string str;
		if(std::getline(file, str)){
			std::vector<uint8_t> data;
			base64_decode(data, str);
			if(config.fromBinary(data))return true;
		}
	}
	return false;
}

bool saveConfig(const bin_protocol::Config &config)
{
	std::ofstream file(CONFIG_FILE, std::ios::out);
	if(file.is_open()){
		auto bin_data = config.toBinary();
		std::string str;
		base64_encode(bin_data, str);
		file << str;
		file.flush();
		file.close();
		return true;
	}
	return false;
}

bool getFlowConfig(bin_protocol::FlowConfiguration &flow_config)
{
	
	std::ifstream file(FLOW_CONFIG_FILE);
	if(file.is_open()){
		
		std::string str;
		if(std::getline(file, str)){
			std::vector<uint8_t> data;
			base64_decode(data, str);
			if(flow_config.fromBinary(data))return true;
		}
	}
	return false;
}

bool saveFlowConfig(const bin_protocol::FlowConfiguration &flow_config)
{
	std::ofstream file(FLOW_CONFIG_FILE, std::ios::out);
	if(file.is_open()){
		auto bin_data = flow_config.toBinary();
		std::string str;
		base64_encode(bin_data, str);
		file << str;
		file.flush();
		file.close();
		return true;
	}
	return false;
}

bool getSchedule(bin_protocol::Schedule &schedule)
{
	std::ifstream file(SCHEDULE_FILE);
	if(file.is_open()){
		std::string str;
		if(std::getline(file, str)){
			std::vector<uint8_t> data;
			base64_decode(data, str);
			if(schedule.fromBinary(data))return true;
		}
	}
	return false;;
}
bool saveSchedule(const bin_protocol::Schedule &schedule)
{
	std::ofstream file(SCHEDULE_FILE, std::ios::out);
	if(file.is_open()){
		auto bin_data = schedule.toBinary();
		std::string str;
		base64_encode(bin_data, str);
		file << str;
		file.flush();
		file.close();
		return true;
	}
	return false;
}

bool getCalibration(bin_protocol::CalibrationSetup &calibration)
{
	std::ifstream file(CALIBRATION_FILE);
	if(file.is_open()){

		std::string str;
		if(std::getline(file, str)){
			std::vector<uint8_t> data;
			base64_decode(data, str);
			if(calibration.fromBinary(data))return true;
		}
	}
	return false;;
}
bool saveCalibration(const bin_protocol::CalibrationSetup &calibration)
{
	std::ofstream file(CALIBRATION_FILE, std::ios::out);
	if(file.is_open()){
		auto bin_data = calibration.toBinary();
		std::string str;
		base64_encode(bin_data, str);
		file << str;
		file.flush();
		file.close();
		return true;
	}
	return false;
}

bin_protocol::Header getHeader(bin_protocol::Type type)
{
	return bin_protocol::Header(FIRMWARE_VERSION, type, std::time(nullptr), getSerialNumber());
}

bin_protocol::Heartbeat getHeartbeat(std::string extra_content)
{
	auto header = getHeader(bin_protocol::HEARTBEAT_MULTI);
	int up_time = getUpTime();
	bin_protocol::Schedule schedule;
	getSchedule(schedule);
	bin_protocol::Config config;
	getConfig(config);
	bin_protocol::FlowConfiguration flow_config;
	getFlowConfig(flow_config);
	int temp = (int) getTemperature();
	std::string state = getRelayState() ? "close" : "open";
	return bin_protocol::Heartbeat(header, up_time, schedule.ID, config.ID, temp, 0, state, extra_content, flow_config.ID);
}

bool getRelayState()
{
	return digitalRead(PIN_RELAY) == HIGH;
}

void openRelay()
{
	digitalWrite(PIN_RELAY, LOW);
}

void closeRelay()
{
	digitalWrite(PIN_RELAY, HIGH);
}

bool isLogReady()
{
	int log_time = 0;
	std::ifstream file(HISTORY_FILE);
	if(file.is_open()){
		std::string line;
		while(getline(file, line)){
			std::smatch results;
			std::regex e("(\\S+) (\\S+)");
			std::regex_search(line, results, e);
			if(results.size() >= 3){
				if(results[1] == "last_log"){
					log_time = std::stoi(results[2]);
				}
			}
		}
		file.close();
	}
	return std::time(nullptr) - log_time > 3600;
}

void rebootSystem()
{
	runCommand("reboot");
}

std::string runCommand(std::string cmd)
{
	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
	if (pipe)
	{
		while (!feof(pipe.get())) {
			if (fgets(buffer.data(), 128, pipe.get()) != NULL)
			result += buffer.data();
		}
	}
	return result;
}


bool handleHBResponse(const std::string &response, std::vector<int> &handled_types, std::vector<int> &unhandled_types)
{
	bool result = false;
	std::vector<uint8_t> data;
	std::stringstream ss(response);
	std::string packet;
	while(ss >> packet){
		std::cout << "SYS_INFO: DECODING \"" << packet << "\""<< std::endl;
		data.clear();
		if(base64_decode(data, packet) == BASE64_NO_ERR){
			if(bin_protocol::isValidData(data)){
				int type = bin_protocol::getTypefromBinary(data);
				bool unhandled_type = false;
				switch(type){
					case bin_protocol::HEADER:{
						//update time
						bin_protocol::Header header;
						if(header.fromBinary(data)){
							result = true;
							setSystemTime(header.timestamp);
						}
						break;
					}
					case bin_protocol::SCHEDULE:{
						bin_protocol::Schedule schedule;
						if(schedule.fromBinary(data)){
							result = true;
							setSystemTime(schedule.header.timestamp);
							std::cout << "SYS_INFO: SAVING SCHEDULE" << std::endl;
							saveSchedule(schedule);
						}
						break;
					}
					case bin_protocol::CONFIG:{
						bin_protocol::Config config;
						if(config.fromBinary(data)){
							result = true;
							setSystemTime(config.header.timestamp);
							std::cout << "SYS_INFO: SAVING CONFIG" << std::endl;
							saveConfig(config);
						}
						break;
					}
					case bin_protocol::FLOW_CONFIG:{
						bin_protocol::FlowConfiguration flow_config;
						if(flow_config.fromBinary(data)){
							result = true;
							setSystemTime(flow_config.header.timestamp);
							std::cout << "SYS_INFO: SAVING FLOW CONFIG" << std::endl;
							saveFlowConfig(flow_config);
						}
						break;
					}
					case bin_protocol::FIRMWARE:{
						
						bin_protocol::Firmware firmware;
						char* firmware_data = NULL;
						int firmware_size;
						if(firmware.fromBinary(data, firmware_data, firmware_size)){
							std::ofstream new_firmware(FIRMWARE_FILE ".gz", std::ofstream::binary);
							new_firmware.write(firmware_data, firmware_size);
							new_firmware.flush();
							new_firmware.close();
							auto md5_string = runCommand("md5sum " FIRMWARE_FILE ".gz");
							std::stringstream converter(md5_string.substr(0,16));
							uint64_t md5_64;
							converter >> std::hex >> md5_64;
							if(firmware.md5_64 == md5_64){
								std::cout << "SYS_INFO: Valid firmware" << std::endl;
								std::string result = runCommand("gunzip -f " FIRMWARE_FILE ".gz");
								std::cout << "SYS_EXEC: " << result << std::endl;
								result = runCommand("chmod 755 " FIRMWARE_FILE);
								std::cout << "SYS_EXEC: " << result << std::endl;
							}
						}
						delete[] firmware_data;
						firmware_data = NULL;
						break;
					}
					case bin_protocol::FLOW_CAL:{
						bin_protocol::CalibrationSetup calibration;
						if(calibration.fromBinary(data)){
							result = true;
							setSystemTime(calibration.header.timestamp);
							std::cout << "SYS_INFO: SAVING CALIBRATION" << std::endl;
							saveCalibration(calibration);
						}
						break;
					}
					default:
						unhandled_type = true;
				}
				if(unhandled_type){
					unhandled_types.push_back(type);
				} else {
					handled_types.push_back(type);
				}
			}
		} else {
			std::cout << "SYS_WARN: Invalid base 64 string: \"" << response << "\"" << std::endl;
		}
	}
	return result;
}