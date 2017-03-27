#include "schedule.h"

#include <string.h>
#include <cmath>
#include <iostream>

bool isWateringNeeded(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, time_t &now)
{
	bool result = false;
	
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

	time_t manual_start_time = midnight + config.manual_start_time * 60;		// manual start time is stored as offset from midnight in minutes
	time_t manual_end_time = manual_start_time + config.manual_end_time * 60;	//manual end time is stored as offset in minutes from start time
	//std::cout << "manual_start_time = " << manual_start_time << "  manual_end_time " << manual_end_time << std::endl;
	time_t effective_date = schedule.effective_date;
	if(manual_start_time <= now && now < manual_end_time){ //Manual run period so swtich relay on
		//std::cout << "Manual2" << std::endl;
		result = true;
	}else if(now >= effective_date)  //TODO move this to normal run so it does not effect the start times for custom programs
		result = isTimeToWater(schedule, config, now, midnight);
	return result;
}


bool get_running_zone(int &program, int &zone, RunType &type, const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight)
{
	bool result = false;
	int i = 0;
	time_t manual_start_time = midnight + config.manual_start_time * 60;		// manual start time is stored as offset from midnight in minutes
	time_t manual_end_time = manual_start_time + config.manual_end_time * 60;	//manual end time is stored as offset in minutes from start time
	
	if(manual_start_time <= now && now < manual_end_time){
		result = true;
		type = MANUAL;
		//std::cout << "Manual" << std::endl;
	} else {
		for(int start : schedule.prgm_start_times)
		{
			//std::cout << "A  " << start << std::endl;
			time_t start_time = midnight + start*60;
			time_t duration = 0;
			for(int runtime : schedule.zone_duration[i])
			{
				duration += runtime*60 + config.station_delay;
			}
			//std::cout << "Duration " << duration << std::endl;
			time_t end_time = start_time + duration; //TODO should log error if end and start times overlap
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
						type = ZONE;
						program = i;
						zone = j;
						//std::cout << zone << "  " << program << "  true" << std::endl;
						break;
					}
					offset_from_program_start -= config.station_delay;
					if(offset_from_program_start < 0) //we are in station delay after run
					{
						result = true;
						type = DELAY;
						program = i;
						zone = j;
						//std::cout << zone << "  " << program << "  false" << std::endl;
						break;
					}
					j++;
				}
			}
			i++;
		}
	}
	return result;
}

bool isTimeToWater(const bin_protocol::Schedule &schedule, const bin_protocol::Config &config, const time_t &now, const time_t &midnight)
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
	//std::cout << "Scheduled " << is_scheduled_day << std::endl;
	int program_idx=0, zone_idx=0;
	RunType type = NONE;
	get_running_zone(program_idx, zone_idx, type, schedule, config, now, midnight);
	//std::cout << "type " << type << std::endl;
	if(type == ZONE || type == DELAY)
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
							if(zone == zone_idx){
								custom_run = true;
								if(custom_should_spinkle && index == program_idx){ //only overrite if type is should sprinkle, Do not water takes priority
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
				if(type == DELAY && !config.remain_closed) result = false; //Open relay for indexing valve systems
			}
		}else if(program_idx == 0 && is_scheduled_day) {//Normal Program A run
			if(type == DELAY && !config.remain_closed) result = false; //Open relay for indexing valve systems
			else result = true;
		}else result = false;
	}
	return result;
}