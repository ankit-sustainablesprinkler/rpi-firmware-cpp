#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <ctime>
//#include "functions.h"
#include "bin_protocol.h"

enum RunType{NONE, DELAY, ZONE, MANUAL, AFTER, BEFORE};

struct run_state_t{
	uint8_t program;
	uint8_t zone;
	RunType type;	
};

time_t schedule_getMidnight(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now);
bool isWateringNeeded(const run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now, time_t midnight);
bool getRunState(run_state_t &mode, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight);
bool isTimeToWater(const run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight);

#endif