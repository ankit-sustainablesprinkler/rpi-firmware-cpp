#include <iostream>
#include <unistd.h>
#include <time.h>
#include <wiringPi.h>

#include "functions.h"
#include "bin_protocol.h"
#include "base64.h"
#include "schedule.h"

using namespace std;
using namespace bin_protocol;

bool runSchedule(const Schedule &schedule, const Config &config);

int main(int argc, char **argv)
{
	wiringPiSetup();
	Schedule schedule;
	Config config;
	bool has_schedule = getSchedule(schedule);
	bool has_config = getConfig(config);
	/*config.manual_end_time = 0;
	
	schedule.prgm_start_times[0] = 1230;
	schedule.prgm_start_times[1] = 1240;
	schedule.prgm_start_times[2] = 1245;
	
	for(int i = 0; i < schedule.zone_duration[0].size(); i++){
		schedule.zone_duration[0][i] = 2;
		schedule.zone_duration[1][i] = 1;
		schedule.zone_duration[2][i] = 1;
		
	}
	
	cout << "============ Entering main loop ================" << endl;
	while(true)
	{
		//lcm.handleTimeout(50);
		runSchedule(schedule, config);
		sleep(1);
	}*/
	string insinuated_b64;
	string b64 = argv[1];
	vector<uint8_t> data;
	base64_decode(data, b64);
	cout << "rtshewgarryjtheg  " << config.fromBinary(data) << endl;
	base64_encode(config.toBinary(), insinuated_b64);
	cout << insinuated_b64 << endl;
	
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