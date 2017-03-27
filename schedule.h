#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <ctime>
//#include "functions.h"
#include "bin_protocol.h"

enum RunType{NONE, DELAY, ZONE, MANUAL};

bool isWateringNeeded(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now);
bool get_running_zone(int &program, int &zone, RunType &type, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight);
bool isTimeToWater(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight);

#endif