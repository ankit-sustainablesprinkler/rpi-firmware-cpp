#include "schedule.h"

#include <string.h>
#include <cmath>
#include <iostream>

time_t schedule_getMidnight(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now)
{
	int timezone = -5; // this needs to be in config

	if(config.use_dst) timezone += 1;
	time_t now_tz = now + timezone*3600;

	struct tm * now_date = localtime(&now_tz);
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
	//std::cout << "POOP A" << std::endl;
	if(manual_start_time <= now && now < manual_end_time){ //Manual run period so swtich relay on
		//std::cout << "Manual2" << std::endl;
		//std::cout << "POOP B" << std::endl;
		result = config.remain_closed;// mechanical systems need to open during manual water.

		//Water Now Feature
		if(!config.remain_closed){
			for(auto custom : schedule.custom_programs)
			{
				int time_diff_mod = (midnight - custom.start_date) % 86400; //programs which are not integer multiples of 24 hours are water now
				if(time_diff_mod && schedule.prgm_start_times.size() > 0){
					if(custom.start_date > midnight){
						run_state_t new_state;
						int modified_time = now + schedule.prgm_start_times[0] *60 - custom.start_date + midnight;
						getRunState(new_state, schedule, config, modified_time, midnight);
						if((new_state.type == ZONE || new_state.type == DELAY) && new_state.program == 0) //Normal Program A run
						{
							if(new_state.type == DELAY) result = false; //Open relay for indexing valve systems
							else result = true;
						}
					}
				}
			}
		}
	}else result = isTimeToWater(state, schedule, config, now, midnight);
	//std::cout << "POOP C" << std::endl;
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
			//if(start < config.manual_start_time) start += 1440;
			
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
	//std::cout << state.type << std::endl;
	return result;
}

bool isTimeToWater(const run_state_t &state, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight)
{
	bool result = false;
	bool is_scheduled_day = false;
	
	
	//TODO add fixed day schedule
	//TODO add flag to indicate legal days to run and shift effective date when conflict occurs
	//if schedule.is_shift:
	int day_of_week = (now / 86400 + 3) % 7; //since UNIX epoch was on a thursday we add 3 to the day so that the 0 maps to monday in the enum
	int difference = (int)roundf((midnight - schedule.effective_date)/86400.0);
	int n = schedule.days; //system runs every nth day.  for a 2 day skip we run once every 3 days
	if(n <= 0) n = 1;

	if(now >= schedule.effective_date){
		if(schedule.days&0x80){
			if(schedule.days&(2<day_of_week)) 
				is_scheduled_day = true;
		} else {
			if(difference % n == 0) //is this an nth day?
				is_scheduled_day = true;
		}
	}
	
	//std::cout << "type " << type << std::endl;

	if(state.type == ZONE || state.type == DELAY) //Normal run
	{
		bool custom_run = false;
		bool custom_run_active = false;
		bool custom_should_spinkle = true;
		for(auto custom : schedule.custom_programs){
			int time_diff_mod = (midnight - custom.start_date) % 86400;
			if(time_diff_mod){ // bug where server does not use the dst setting for the custom program start date.
				if(!((midnight - custom.start_date + 3600) % 86400)){
					custom.start_date -= 3600;
					time_diff_mod = 0;
				} else if(!((midnight - custom.start_date - 3600) % 86400)){
					custom.start_date += 3600;
					time_diff_mod = 0;
				}
			}

			if(time_diff_mod == 0){
				int time_diff_days = (midnight - custom.start_date)/86400; // programs which are not integer multiples of 24 hours are water now and are ignored here
				if(custom.period < 1) custom.period = 1;

				bool program_active = 
						(time_diff_days >= 0 && midnight < (custom.start_date + custom.days)) || //program is active

						(!custom.uses_schedule && custom.should_override && !(time_diff_days%custom.period)) ||//the custom program overrides the regular schedule in all cases.

						(!custom.uses_schedule && !custom.should_override && !is_scheduled_day && !((difference % n)%custom.period)) ||//custom program is syncronized to regular program but regular takes priority
						
						(custom.uses_schedule && is_scheduled_day); //custom program runs only on schedule days so skip all others

				/*
				if(time_diff_days < 0) continue;//custom program is in the future or not schedule
				
				if(!custom.uses_schedule && custom.should_override && time_diff_days%custom.period) continue; //custom program runs according to custom period regardless of schedule

				bool no_run_days = is_scheduled_day || (difference - custom.period + 1)%n;
				if(!custom.should_override && (custom.uses_schedule || no_run_days)) continue; //schedule overrides custom program on these days, so skip

				if(custom.uses_schedule && !is_scheduled_day) continue; //custom program runs only on schedule days so skip all others
				
				if(time_diff_days < custom.days) { //program is active
				*/
				if(program_active){
					int index = 0;
					for(auto program : custom.zones){
						for(auto zone : program){
							if(zone == state.zone){
								custom_run = true;
								if(custom_should_spinkle && index == state.program){ //only override if type is should sprinkle, Do not water takes priority
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