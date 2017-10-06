#include "schedule.h"

#include <string.h>
#include <cmath>
#include <iostream>

time_t schedule_getMidnight(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now)
{
	int timezone = -4; // this needs to be in config

	struct tm * now_date = localtime(&now);
	struct tm midnight_date;
	memset(&midnight_date, 0, sizeof(midnight_date));
	midnight_date.tm_year = now_date->tm_year;
	midnight_date.tm_mon = now_date->tm_mon;
	midnight_date.tm_mday = now_date->tm_mday;
	midnight_date.tm_hour = -timezone;
	time_t midnight = mktime(&midnight_date);
	//std::cout << "MIDNIGHT = " << midnight << std::endl;
	//std::cout << "haeheqh " << midnight << "  " << schedule.prgm_start_times.size() << std::endl;

	now += config.system_time_offset; //adjust current time to compensate for clock drift

	if(now - midnight < (config.manual_start_time * 60) && schedule.prgm_start_times[0] > config.manual_start_time)
	{
		midnight -= 86400; // If program A starts before midnight then when the day rolls over 
						  // we need to set midnight back one day so the rest of the irrigation 
						  // cycle can run.  this condition is nullified if Program A start time is after midnight.
		//std::cout << "Subtracting 1 day" << std::endl;
	}
	return midnight;
}

bool isWateringNeeded(const run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now, time_t midnight)
{
	bool result = false;
	


	time_t manual_start_time = midnight + config.manual_start_time * 60;		// manual start time is stored as offset from midnight in minutes
	time_t manual_end_time = manual_start_time + config.manual_end_time * 60;	//manual end time is stored as offset in minutes from start time
	//std::cout << "manual_start_time = " << manual_start_time << "  manual_end_time " << manual_end_time << std::endl;
	time_t effective_date = schedule.effective_date;
	if(manual_start_time <= now && now < manual_end_time){ //Manual run period so swtich relay on
		//std::cout << "Manual2" << std::endl;
		result = config.remain_closed;// mechanical systems need to open during manual water
	}else if(now >= effective_date)  //TODO move this to normal run so it does not effect the start times for custom programs
		result = isTimeToWater(state, schedule, config, now, midnight);
	return result;
}


bool getRunState(run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight)
{
	bool result = false;
	int i = 0;
	time_t manual_start_time = midnight + config.manual_start_time * 60;		// manual start time is stored as offset from midnight in minutes
	time_t manual_end_time = manual_start_time + config.manual_end_time * 60;	//manual end time is stored as offset in minutes from start time
	
	int last_program_end_time=0, first_program_start_time=manual_end_time+1440;
	
	if(manual_start_time <= now && now < manual_end_time){
		result = true;
		state.type = MANUAL;
		//std::cout << "Manual" << std::endl;
	} else {
		for(int start : schedule.prgm_start_times)
		{
			//std::cout << "A  " << start << std::endl;
			if(start < config.manual_start_time) start += 1440;
			
			if(first_program_start_time < start) first_program_start_time = start;
			
			time_t start_time = midnight + start*60;
			time_t duration = 0;
			for(int runtime : schedule.zone_duration[i])
			{
				duration += runtime*60 + config.station_delay;
			}
			//std::cout << "Duration " << duration << std::endl;
			time_t end_time = start_time + duration; //TODO should log error if end and start times overlap
			if(last_program_end_time < end_time) last_program_end_time = end_time;
			//std::cout << "start " << start_time << "   end " << end_time << std::endl;
			if(start_time <= now && now < end_time) //this is the current program running
			{
				time_t offset_from_program_start = now - start_time; // how far are we in the current program?
				int j = 0;
				while(j < schedule.zone_duration[i].size())
				{
					offset_from_program_start -= schedule.zone_duration[i][j]*60;
					if(offset_from_program_start < 0) //we are in a zone run
					{
						
						result = true;
						state.type = ZONE;
						state.program = i;
						state.zone = j;
						//std::cout << zone << "  " << program << "  true" << std::endl;
						break;
					}
					offset_from_program_start -= config.station_delay;
					if(offset_from_program_start < 0) //we are in station delay after run
					{
						result = true;
						state.type = DELAY;
						state.program = i;
						state.zone = j;
						//std::cout << zone << "  " << program << "  false" << std::endl;
						break;
					}
					j++;
				}
			}
			i++;
		}
		if(!result){
			//std::cout << "ERTBHGWEBW" << std::endl;
			if(manual_end_time <= now && now < first_program_start_time) state.type = BEFORE;
			else if(last_program_end_time <= now && now < 86400+manual_start_time) state.type = AFTER;
			//std::cout << state.type << "   " <<  last_program_end_time << "  " << now << "   " << manual_start_time << std::endl;
		}
	}
	return result;
}

bool isTimeToWater(const run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight)
{
	bool result = false;
	bool is_scheduled_day = false;
	
	
	//TODO add fixed day schedule
	//TODO add flag to indicate legal days to run and shift effective date when conflict occurs
	//if schedule.is_shift:
	int difference = (midnight - schedule.effective_date)/86400;
	int n = schedule.days + 1; //system runs every nth day.  for a 2 day skip we run once every 3 days
	if(difference % n == 0) //is this an nth day?
		is_scheduled_day = true;
	
	//std::cout << "type " << type << std::endl;
	if(state.type == ZONE || state.type == DELAY)
	{
		bool custom_run = false;
		bool custom_run_active = false;
		bool custom_should_spinkle = true;
		for(auto custom : schedule.custom_programs)
		{
			int time_diff_days = (int)roundf((midnight - custom.start_date)/86400.0);
			if(time_diff_days < 0) continue;//custom program is in the future
			else{
				if(time_diff_days < custom.days) { //program is active
					int index = 0;
					for(auto program : custom.zones){
						for(auto zone : program){
							if(zone == state.zone){
								custom_run = true;
								if(custom_should_spinkle && index == state.program){ //only overrite if type is should sprinkle, Do not water takes priority
									custom_run_active = true;
									custom_should_spinkle = custom.should_sprinkle;
								}
							}
						}
						index ++;
					}
				}
			}
		}
		if(custom_run){
			if(custom_run_active){
				result = custom_should_spinkle;
				if(state.type == DELAY && !config.remain_closed) result = false; //Open relay for indexing valve systems
			}
		}else if(state.program == 0 && is_scheduled_day) {//Normal Program A run
			if(state.type == DELAY && !config.remain_closed) result = false; //Open relay for indexing valve systems
			else result = true;
		}else result = false;
	}
	return result;
}