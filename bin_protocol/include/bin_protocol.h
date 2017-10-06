#ifndef _BIN_PROTOCOL_H_
#define _BIN_PROTOCOL_H_
#include <vector>
#include <stdint.h>
#include <string>
#include "crc16.h"

#define HEADER_SIZE 16
#define HEARTBEAT_SIZE (HEADER_SIZE + 11)
#define LOG_ENTRY_SIZE 7

#define CUSTOM_LESS_WATER 0
#define CUSTOM_FLOWER 1
#define CUSTOM_FLOWER_SKIP 2
#define CUSTOM_NO_WATER 3
#define CUSTOM_FERTILIZER 4
#define CUSTOM_MORE_WATER 5
#define CUSTOM_SOD 6

/*PROGRAMS=[None]*7
PROGRAMS[CUSTOM_LESS_WATER] = [1]
PROGRAMS[CUSTOM_FLOWER] = [1]
PROGRAMS[CUSTOM_FLOWER_SKIP] = [1]
PROGRAMS[CUSTOM_NO_WATER] = [0,1,2]
PROGRAMS[CUSTOM_FERTILIZER] = [0,1]
PROGRAMS[CUSTOM_MORE_WATER] = [0,1]
PROGRAMS[CUSTOM_SOD] = [0,1,2]*/

namespace bin_protocol {
	
enum Type {HEADER =0, HEARTBEAT=1,SCHEDULE = 2, CONFIG = 3, FIRMWARE = 4, LOG = 5, FLOW = 6, FLOW_CAL = 7, FEEDBACK=8, FLOW_CONFIG = 9};

class Header
{
public:
	Header(int version = 0, Type type = HEADER, int timestamp = 0, int ID = 0);
	
	std::vector<uint8_t> toBinary() const;
	
	bool fromBinary(const std::vector<uint8_t> &data);
	
	int content_length;
	int version;
	Type type;
	int timestamp;
	uint32_t ID;
	uint16_t crc16;
};


//validates the binary data after base64 decoding
bool isValidData(const std::vector<uint8_t> &data);

//calculates the crc of the data string and places it into the header and the crc section of the data string
int calculateCRC(std::vector<uint8_t> &data);

//gets the content_length parameter from the data string
int getSizefromBinary(const std::vector<uint8_t> &data);

//gets the messege type parameter from the data string
int getTypefromBinary(const std::vector<uint8_t> &data);

//gets the messege version parameter from the data string
int getVersionfromBinary(const std::vector<uint8_t> &data);

//gets the device ID parameter from the data string
int getIDfromBinary(const std::vector<uint8_t> &data);

void putSizeIntoData(std::vector<uint8_t> &data);

class Heartbeat
{
public:
	Heartbeat();
	Heartbeat(Header header, int up_time = 0, int schedule_id = 0, int config_id = 0, int temperature = 0, int signal = 0, std::string state = "open", std::string extra_content = "");
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	int up_time;
	int schedule_id;
	int config_id;
	int temperature;
	int signal;
	std::string state;
	std::string extra_content;
	Header header;
};

struct log_point_t
{
	std::string running;
	int zone;
	int program;
	float flow;
	float voltage;
	float current;
	int time;
};

class Log
{
public:
	Log(){}
	Log(Header header, std::vector<log_point_t> &log_points);
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	Header header;
	std::vector<log_point_t> log_points;
};

struct custom_t
{
	std::vector<std::vector<uint8_t> > zones;
	uint32_t start_date;
	uint8_t days;
	uint8_t period;
	bool should_sprinkle;
};

class Schedule
{
public:
	Schedule();
	Schedule(Header header, int ID=0, int effective_date=0, int days=0, bool is_shift=false, std::vector<int> prgm_start_times = std::vector<int>(),
		std::vector<std::vector<int> > zone_duration = std::vector<std::vector<int>>(), std::vector<custom_t> custom_programs = std::vector<custom_t>());
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	bool isValid();
	Header header;
	int ID;
	int effective_date;
	int days;
	bool is_shift;
	std::vector<int> prgm_start_times;
	std::vector<std::vector<int> > zone_duration;
	std::vector<custom_t> custom_programs;
};



class Config
{
public:
	Config();
	Config(Header header, int ID = 0, int manual_start_time = 0, int manual_end_time = 0, int heartbeat_period = 120, int16_t system_time_offset = 0,\
		int station_delay = 0, bool remain_closed = true);
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	Header header;
	int ID;
	int manual_start_time;
	int manual_end_time;
	int heartbeat_period;
	int16_t system_time_offset;
	int station_delay;
	bool remain_closed;
};
/*
class Config
{
public:
	Config();
	Config(Header header, int ID = 0, int manual_start_time = 0, int manual_end_time = 0, int heartbeat_period = 120, int16_t system_time_offset = 0,\
		int station_delay = 0, bool remain_closed = true);
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	Header header;
	int ID;
	int manual_start_time;
	int manual_end_time;
	int heartbeat_period;
	int16_t system_time_offset;
	int station_delay;
	bool remain_closed;
	float flow_k;
	float flow_offset;
	bool flow_fitted;
};
*/
struct feedback_log_point_t
{
	float voltage;
	float current;
	float flow;
	uint8_t duration;
	bool run;
};

class Feedback
{
public:
	Feedback();
	Feedback(Header header, uint16_t manual_time=0, uint16_t after_time=0, uint16_t before_time=0, std::vector<std::vector<feedback_log_point_t>> zone_runs=std::vector<std::vector<feedback_log_point_t>>());
	std::vector<uint8_t> toBinary() const;
	bool fromBinary(const std::vector<uint8_t> &data);
	Header header;
	uint16_t before_time;
	uint16_t manual_time;
	uint16_t after_time;
	std::vector<std::vector<feedback_log_point_t>> zone_runs;	
};

class Firmware
{
public:
	Firmware();
	Firmware(const std::vector<uint8_t> &data);
	std::vector<uint8_t> toBinary(const char *data, int size) const;
	bool fromBinary(const std::vector<uint8_t> &data, char *firmware, int &size);
	bool isValid();
	uint64_t md5_64;
	Header header;
};

}
#endif