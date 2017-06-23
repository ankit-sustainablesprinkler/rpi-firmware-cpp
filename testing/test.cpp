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

//bool runSchedule(const Schedule &schedule, const Config &config);

int main(int argc, char **argv)
{
	wiringPiSetup();
	Schedule schedule;
	Config config;
	bool has_schedule = getSchedule(schedule);
	bool has_config = getConfig(config);
	
	Header header;
	header.type = bin_protocol::FEEDBACK;
	
	Feedback feedback(header);
	
	feedback.before_time = 560;
	feedback.after_time =782;
	feedback.manual_time = 265;
	
	feedback.zone_runs.resize(3);
	for(int i = 0; i < 3; i++){
		feedback.zone_runs[i].resize(4);
		for(int j = 0; j < 4; j++){
			feedback.zone_runs[i][j].voltage = i*j;
			feedback.zone_runs[i][j].current = (i+j)*0.1f;
			feedback.zone_runs[i][j].flow = i+j+20;
			feedback.zone_runs[i][j].duration = (1+i+j)*5;
			feedback.zone_runs[i][j].run = (i==1 || j == 1);
			
		}
	}
	
	vector<uint8_t> data = feedback.toBinary();
	
	string b64;
	
	base64_encode(data, b64);
	
	cout << b64 << endl;
	
	vector<uint8_t> newdata;
	
	base64_decode(newdata, b64);
	
	Feedback newfeedback;
	if(newfeedback.fromBinary(newdata)){
		cout << newfeedback.zone_runs.size() << endl;
		int i = 0, j=0;
		for(auto program : newfeedback.zone_runs){
			j=0;
			for(auto run : program){
				cout << "Prgm: " << i << ", zone: " << j << ", V: " << run.voltage << ", I: " << run.current << ", F: " << run.flow << ", D: " << (int)run.duration << ", R: " << run.run << endl;
				j++;
			}
			i++;
		}
	}
	else {
		cout << "Fail" << endl;
	}
	
	
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
	/*string insinuated_b64;
	string b64 = argv[1];
	vector<uint8_t> data;
	base64_decode(data, b64);
	cout << "rtshewgarryjtheg  " << config.fromBinary(data) << endl;
	base64_encode(config.toBinary(), insinuated_b64);
	cout << insinuated_b64 << endl;*/
	
	return 0;
}

/*bool runSchedule(const Schedule &schedule, const Config &config)
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
}*/