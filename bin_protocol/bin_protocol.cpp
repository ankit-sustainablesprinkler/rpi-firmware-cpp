#include "bin_protocol.h"
#include "string.h"

#include <iostream>
#include <tuple>

namespace bin_protocol{
bool isValidData(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		std::vector<uint8_t> dataToCheck(data.begin()+2, data.end());
		uint16_t crc = data[0] | (data[1] << 8);
		uint16_t new_crc = CRC16(dataToCheck, 0);
		return crc == new_crc;
	} else {
		return false;
	}
}

int calculateCRC(std::vector<uint8_t> &data)
{
	std::vector<uint8_t> dataToCheck(data.begin()+2, data.end());
	uint16_t crc = CRC16(dataToCheck, 0);
	data[0] = crc & 0xFF;
	data[1] = (crc >> 8) & 0xFF;
	return crc;
}

int getSizefromBinary(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		return data[8] | (data[9] << 8) | (data[10] << 16) | (data[11] << 24);
	} else return 0;
}

int getTypefromBinary(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		return data[3];
	} else return 0;
}

int getVersionfromBinary(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		return data[2];
	} else return 0;
}

int getIDfromBinary(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		return data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
	} else return 0;
}

void putSizeIntoData(std::vector<uint8_t> &data)
{
	uint32_t size = data.size();
	data[8] = size & 0xFF;
	data[9] = (size >> 8) & 0xFF;
	data[10] = (size >> 16) & 0xFF;
	data[11] = (size >> 24) & 0xFF;
}

Header::Header(int version, Type type, int timestamp, int ID)
{
	this->crc16 = 0;
	this->version = version;
	this->type = type;
	this->timestamp = timestamp;
	this->content_length = 0;
	this->ID = ID;
}

std::vector<uint8_t> Header::toBinary() const
{
	std::vector<uint8_t> data;
	data.push_back(this->crc16 & 0xFF);
	data.push_back((this->crc16 >> 8) & 0xFF);
	data.push_back(this->version & 0xFF);
	data.push_back(this->type & 0xFF);
	data.push_back(this->timestamp & 0xFF);
	data.push_back((this->timestamp >> 8) & 0xFF);
	data.push_back((this->timestamp >> 16) & 0xFF);
	data.push_back((this->timestamp >> 24) & 0xFF);
	data.push_back(this->content_length & 0xFF);
	data.push_back((this->content_length >> 8) & 0xFF);
	data.push_back((this->content_length >> 16) & 0xFF);
	data.push_back((this->content_length >> 24) & 0xFF);
	data.push_back(this->ID & 0xFF);
	data.push_back((this->ID >> 8) & 0xFF);
	data.push_back((this->ID >> 16) & 0xFF);
	data.push_back((this->ID >> 24) & 0xFF);

	//this->content_length = data.size();
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool Header::fromBinary(const std::vector<uint8_t> &data)
{
	if(data.size() >= HEADER_SIZE){
		this->crc16 = data[0] | (data[1] << 8);
		this->version = data[2];
		this->type = (Type)data[3];
		this->timestamp = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
		this->content_length = getSizefromBinary(data);
		this->ID = data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24);
		return true;
	} else return false;
}

Heartbeat::Heartbeat()
{
	this->header = 0;
	this->up_time = 0;
	this->schedule_id = 0;
	this->config_id = 0;
	this->temperature = 0;
	this->signal = 0;
	this->state = "";
	this->extra_content = "";
}

Heartbeat::Heartbeat(Header header, int up_time, int schedule_id, int config_id, int temperature, int signal, std::string state, std::string extra_content, int flow_id)
{
	this->header = header;
	this->header.type = HEARTBEAT;
	this->up_time = up_time;
	this->schedule_id = schedule_id;
	this->config_id = config_id;
	this->temperature = temperature;
	this->signal = signal;
	this->state = state;
	this->extra_content = extra_content;
	this->flow_id = flow_id;
}

std::vector<uint8_t> Heartbeat::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	data.push_back(this->up_time & 0xFF);
	data.push_back((this->up_time >> 8) & 0xFF);
	data.push_back((this->up_time >> 16) & 0xFF);
	data.push_back((this->up_time >> 24) & 0xFF);
	data.push_back(this->schedule_id & 0xFF);
	data.push_back(this->config_id & 0xFF);
	data.push_back(this->flow_id&0xFF);
	data.push_back(this->temperature & 0xFF);
	data.push_back(this->signal & 0xFF);
	data.push_back(this->state == "close" ? 1 : 0);
	data.push_back(this->extra_content.size() & 0xFF);
	data.push_back((this->extra_content.size() >> 8) &0xFF);
	
	if(this->extra_content.size() > 0){
		data.insert(data.end(), this->extra_content.begin(), this->extra_content.end());
	}
	//this->header.content_length = data.size();
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool Heartbeat::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		int size = data[HEARTBEAT_SIZE - 2] | (data[HEARTBEAT_SIZE-1] << 8);
		if(getSizefromBinary(data) == HEARTBEAT_SIZE + size)
		{
			if(this->header.fromBinary(data)){
				//use HEADER_SIZE as offset
				this->up_time = data[HEADER_SIZE] | (data[HEADER_SIZE+1] << 8) | (data[HEADER_SIZE+2] << 16) | (data[HEADER_SIZE+3] << 24);
				this->schedule_id = data[HEADER_SIZE+4];
				this->config_id = data[HEADER_SIZE+5];
				this->flow_id = data[HEADER_SIZE + 6];
				this->temperature = data[HEADER_SIZE+7];
				this->signal = data[HEADER_SIZE+8];
				this->state = (data[HEADER_SIZE+9] & 0x01) == 0x01 ? "close" : "open";
				this->extra_content.clear();
				if(size > 0){
					this->extra_content.insert(extra_content.end(), data.begin()+HEARTBEAT_SIZE, data.end());
				} 
				return true;
			} else return false;
		} else return false;
	} else return false;
	
}
/*
class MessageFirmware:
	def __init__(self, header=MessageHeader(), fileData=bytearray()):
		this->header = header
		this->fileData = fileData

	def getBinary(self):
		data = bytearray()
		data.extend(this->header.toBinary())

		fileLength = len(this->fileData)
		data.append(fileLength & 0xFF)
		data.append((fileLength >> 8) & 0xFF)
		data.append((fileLength >> 16) & 0xFF)
		data.append((fileLength >> 24) & 0xFF)
		data.extend(this->fileData)

		this->header.content_length = len(data)
		this->header.putSizeIntoData(data)
		calculateCRC(data, this->header)
		return data

	def fromBinary(self, data):
		if isValidData(data):
			if getSizefromBinary(data) == len(data):
				this->header = MessageHeader()
				if this->header.fromBinary(data):
					this->fileData = data[HEADER_SIZE + 4:]
					return True
				else: return False
			else: return False
		else: return False*/

Log::Log(Header header, std::vector<log_point_t> &log_points)
{
	this->header = header;
	this->log_points = log_points;
}

std::vector<uint8_t> Log::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	int length = 0;
	data.push_back(0);
	data.push_back(0);
	for(auto row : this->log_points){
		if(row.running != "zone"){
			if(row.running == "manual") data.push_back(0b11111101);
			else if(row.running == "off") data.push_back(0b11111100);
			else continue;
		} else if(row.running == "zone"){
			if(row.zone > 47 || row.program > 3)
				continue;
			else
				data.push_back(((row.zone & 0x3F) << 2) | (row.program & 0x03));
		} else continue;
		data.push_back((int)row.flow & 0xFF);
		data.push_back((int)(row.current * 0.2) & 0xFF);
		data.push_back((int)(row.voltage * 5.0) & 0xFF);
		data.push_back(row.time & 0xFF);
		data.push_back((row.time >> 8) & 0xFF);
		data.push_back((row.time >> 16) & 0xFF);
		data.push_back((row.time >> 24) & 0xFF);
		length ++;
	}
	data[HEADER_SIZE] = length & 0xFF;
	data[HEADER_SIZE + 1] = (length >> 8) & 0xFF;
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool Log::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data))
	{
		if(getSizefromBinary(data) == data.size())
		{
			if(this->header.fromBinary(data))
			{
				int i = 0;
				int log_count = data[HEADER_SIZE] | (data[HEADER_SIZE + 1] << 8);
				int offset = HEADER_SIZE + 2;
				log_point_t log;
				this->log_points.clear();
				while(i < log_count)
				{
					uint8_t running_byte = data[offset];
					if((running_byte & 0b11111100) == 0b11111100){
						if(running_byte == 0b11111100)
							log.running = "off";
						else if(running_byte == 0b11111101)
							log.running = "manual";
						else continue;
					} else {
						log.zone = (running_byte >> 2) & 0b00111111;
						log.program = running_byte & 0b00000011;
						log.running = "zone";
					}
					log.flow = data[offset + 1];
					log.current = data[offset + 2] / 0.2;
					log.voltage = data[offset + 3] / 5.0;
					log.time = data[offset + 4] | (data[offset + 5] << 8) | (data[offset + 6] << 16) | (data[offset + 7] << 24);

					this->log_points.push_back(log);

					offset += 8;
					i++;
				} return true;
			} else return false;
		} else return false;
	} else return false;
}

Schedule::Schedule()
{
	this->ID = 0;
	this->effective_date = 0;
	this->days = 0;
	this->is_shift = 0;
}
Schedule::Schedule(Header header, int ID, int effective_date, int days, bool is_shift, std::vector<int> prgm_start_times, \
                   std::vector<std::vector<int> > zone_duration, std::vector<custom_t> custom_programs)
{
	this->header = header;
	this->ID = ID;
	this->effective_date = effective_date;
	this->days = days;
	this->is_shift = is_shift;
	this->prgm_start_times = prgm_start_times;
	this->zone_duration = zone_duration;
	this->custom_programs = custom_programs;
}

std::vector<uint8_t> Schedule::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	data.push_back(this->ID & 0xFF);
	int prgm_count = this->prgm_start_times.size();
	data.push_back(prgm_count & 0xFF);
	int zone_count = 0; 
	if(prgm_count) zone_count = this->zone_duration[0].size();
	data.push_back(zone_count & 0xFF);
	data.push_back(this->effective_date & 0xFF);
	data.push_back((this->effective_date >> 8) & 0xFF);
	data.push_back((this->effective_date >> 16) & 0xFF);
	data.push_back((this->effective_date >> 24) & 0xFF);
	data.push_back((this->days & 0x7F) | (this->is_shift ? 0x80 : 0x00));
	
	for(int time : this->prgm_start_times)
	{
		data.push_back(time & 0xFF);
		data.push_back((time >> 8) & 0xFF);
	}
	for(auto program : this->zone_duration)
	{
		for(auto duration : program)
		{
			data.push_back(duration & 0xFF);
		}
	}
	int size_idx = data.size();
	data.push_back(0);
	int prog_count = this->zone_duration.size();
	if(prog_count > 0 && zone_count > 0){
		for(const auto &custom : this->custom_programs)
		{
			uint32_t start_date = custom.start_date;
			uint16_t days = custom.days;
			uint8_t period = custom.period;
			uint8_t should_spinkle = custom.should_sprinkle ? 1 : 0;
			int size = (prog_count*zone_count)/8 + !!(prog_count*zone_count); //round up
			std::vector<uint8_t> zone_data(size);
			int prog_num = 0;
			//Some more bit shifting magic
			for(const std::vector<uint8_t> &program : custom.zones){
				for(uint8_t zone_idx : program)
				{
					if(zone_idx < zone_count && prog_num < prog_count){
						int bit_num = prog_num * zone_count + zone_idx; //this is the bit position of this zone in the byte array
						zone_data[bit_num / 8] |= (1 << (bit_num % 8));
					}
				}
				prog_num++;
			}

			data.push_back(start_date & 0xFF);
			data.push_back((start_date >> 8) & 0xFF);
			data.push_back((start_date >> 16) & 0xFF);
			data.push_back((start_date >> 24) & 0xFF);
			data.push_back(days & 0xFF);
			data.push_back(((days>>8) & 0x01) | ((should_spinkle << 1) & 0x02) | ((period << 2) & 0xFC));
			data.insert(data.end(),zone_data.begin(), zone_data.end());
			data[size_idx]++;
		}
	}

	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}


bool Schedule::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data))
	{
		if(getSizefromBinary(data) == data.size())
		{
			if(this->header.fromBinary(data))
			{
				this->ID = data[HEADER_SIZE];
				int program_count = data[HEADER_SIZE + 1];
				int zone_count = data[HEADER_SIZE + 2];
				int zone_data_length = (program_count*zone_count)/8 + !!(program_count*zone_count);
				this->effective_date = data[HEADER_SIZE + 3] | (data[HEADER_SIZE+4] << 8) | (data[HEADER_SIZE+5] << 16) | (data[HEADER_SIZE+6] << 24);
				this->days = data[HEADER_SIZE + 7] & 0x7F;
				this->is_shift = (data[HEADER_SIZE+7]&0x80) == 0 ? false : true;
				int offset = HEADER_SIZE + 8;
				int i = 0;
				this->prgm_start_times.clear();
				this->zone_duration.clear();
				this->custom_programs.clear();
				while(i < program_count)
				{
					this->prgm_start_times.push_back(data[offset + i*2] | (data[offset + i*2 + 1] << 8));
					i++;
				}
				i = 0;
				offset = HEADER_SIZE + 8 + program_count*2;
				while(i < program_count)
				{
					int j = 0;
					std::vector<int> program;
					while(j < zone_count)
					{
						program.push_back(data[offset + i*zone_count + j]);
						j++;
					}
					this->zone_duration.push_back(program);
					i++;
				}
				offset = HEADER_SIZE + 8 + program_count*2 + zone_count*program_count;
				int custom_length = data[offset];
				offset++;
				int custom_idx = 0;
				while(custom_idx < custom_length)
				{
					custom_t custom;
					custom.start_date = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
					custom.days = data[offset + 4] | ((data[offset + 5] & 0x01) << 8);
					custom.should_sprinkle = (data[offset + 5] & 0x02) == 0 ? false : true;
					custom.period = (data[offset + 5] >> 2) & 0x3F;
					offset += 6;
					int program_idx = 0;
					while(program_idx < program_count)
					{
						int zone_idx = 0;
						custom.zones.push_back(std::vector<uint8_t>());
						while(zone_idx < zone_count)
						{
							int bit_num = program_idx*zone_count+zone_idx;
							if((data[offset + bit_num / 8] & 1 << (bit_num % 8)) > 0)
							{
								custom.zones[program_idx].push_back(zone_idx);
							}
							zone_idx ++;
						}
						program_idx ++;
					}
					offset += zone_data_length;
					this->custom_programs.push_back(custom);
					custom_idx++;
				}

				return true;
			} else return false;
		} else return false;
	} else return false;
}

bool Schedule::isValid(){
	return this->ID != 0 && this->effective_date != 0 && this->prgm_start_times.size() > 0
	       && this->zone_duration.size() > 0;
}

Config::Config()
{	
	this->ID = 0;
	this->manual_start_time = 0;
	this->manual_end_time = 0;
	this->heartbeat_period = 0;
	this->system_time_offset = 0;
	this->station_delay = 0;
	this->remain_closed = 0;
	this->flow_fitted = 0;
	this->pump_fitted = false;
	this->time_drift_thr = 0;
	this->use_dst = false;
	this->current_on_thr = 0;
}

Config::Config(Header header, int ID, int manual_start_time, int manual_end_time, int heartbeat_period, int16_t system_time_offset,\
               int station_delay, bool remain_closed, bool flow_fitted, bool pump_fitted, uint8_t time_drift_thr, bool use_dst, uint8_t current_on_thr)
{
	this->header = header;
	this->ID = ID;
	this->manual_start_time = manual_start_time;
	this->manual_end_time = manual_end_time;
	this->heartbeat_period = heartbeat_period;
	this->system_time_offset = system_time_offset;
	this->station_delay = station_delay;
	this->remain_closed = remain_closed;
	this->flow_fitted = flow_fitted;
	this->pump_fitted = pump_fitted;
	this->time_drift_thr = time_drift_thr;
	this->use_dst = use_dst;
	this->current_on_thr = current_on_thr;


}

std::vector<uint8_t> Config::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());

	data.push_back(this->ID & 0xFF);
	data.push_back(this->manual_start_time & 0xFF);
	data.push_back((this->manual_start_time >> 8) & 0xFF);
	data.push_back(this->manual_end_time & 0xFF);
	data.push_back((this->manual_end_time >> 8) & 0xFF);
	data.push_back(this->heartbeat_period & 0xFF);
	data.push_back((this->heartbeat_period >> 8) & 0xFF);
	data.push_back(this->system_time_offset & 0xFF);
	data.push_back((this->system_time_offset >> 8) & 0xFF);
	data.push_back(this->station_delay & 0xFF);
	data.push_back((this->station_delay >> 8) & 0xFF);
	data.push_back(this->time_drift_thr);
	data.push_back((this->remain_closed ? 0x01 : 0x00) | (this->flow_fitted ? 0x02 : 0x00) | (this->pump_fitted ? 0x04 : 0x00));
	data.push_back(this->current_on_thr & 0xFF);

	//this->header.content_length = data.size();
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool Config::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data))
	{
		if(getSizefromBinary(data) == data.size())
		{
			if(this->header.fromBinary(data))
			{
				this->ID = data[HEADER_SIZE];
				this->manual_start_time = data[HEADER_SIZE+1] | (data[HEADER_SIZE+2] << 8);
				this->manual_end_time = data[HEADER_SIZE+3] | (data[HEADER_SIZE+4] << 8);
				this->heartbeat_period = data[HEADER_SIZE+5] | (data[HEADER_SIZE+6] << 8);
				this->system_time_offset = data[HEADER_SIZE+7] | (data[HEADER_SIZE+8] << 8);
				this->station_delay = (data[HEADER_SIZE+9] | (data[HEADER_SIZE+10] << 8));
				this->time_drift_thr = data[HEADER_SIZE + 11];
				this->remain_closed = (data[HEADER_SIZE + 12] & 0x01) != 0x00;
				this->flow_fitted = (data[HEADER_SIZE + 12] & 0x02) != 0x00;
				this->pump_fitted = (data[HEADER_SIZE + 12] & 0x04) != 0x00;
				this->use_dst = (data[HEADER_SIZE + 12] & 0x08) != 0x00;
				this->current_on_thr = data[HEADER_SIZE+13];
				return true;
			} else return false;
		} else return false;
	} else return false;
}

Feedback::Feedback(){
	
	this->header.type=FEEDBACK;
	this->manual_time=0;
	this->before_time=0;
	this->after_time=0;
}

Feedback::Feedback(Header header, uint16_t manual_time, uint16_t after_time, uint16_t before_time, std::vector<std::vector<feedback_log_point_t>> zone_runs)
{
	this->header=header;
	this->header.type=FEEDBACK;
	this->manual_time=manual_time;
	this->after_time=after_time;
	this->before_time=before_time;
	this->zone_runs=zone_runs;
}

std::vector<uint8_t> Feedback::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());

	data.push_back(this->manual_time & 0xFF);
	data.push_back((this->manual_time >> 8) & 0xFF);
	data.push_back(this->before_time & 0xFF);
	data.push_back((this->before_time >> 8) & 0xFF);
	data.push_back(this->after_time & 0xFF);
	data.push_back((this->after_time >> 8) & 0xFF);
	data.push_back(this->zone_runs.size() & 0xFF);
	if(zone_runs.size() > 0){
		data.push_back(zone_runs[0].size());

		for(auto program : this->zone_runs){
			for(int i = 0; i < zone_runs[0].size(); i++){
				if(i < program.size()){
					int val;
					val = program[i].voltage * 5.0f;
					if(val > 255) val = 255;
					if(val < 0) val = 0;
					data.push_back(val);
					val = program[i].xfmr_voltage * 5.0f;
					if(val > 255) val = 255;
					if(val < 0) val = 0;
					data.push_back(val);
					val = program[i].current * 100.0f;
					if(val > 255) val = 255;
					if(val < 0) val = 0;
					data.push_back(val);
					val = program[i].flow;
					if(val > 255) val = 255;
					if(val < 0) val = 0;
					data.push_back(val);
					data.push_back(program[i].duration);
					data.push_back(program[i].volt_duration);
				} else {
					data.push_back(0);
					data.push_back(0);
					data.push_back(0);
					data.push_back(0);
					data.push_back(0);
				}
			}
		}

	} else {
		data.push_back(0);
	}
	
	data.push_back(manual_runs.size());
	if(manual_runs.size()){
		for(auto manual_run: manual_runs){
			int timestamp = std::get<0>(manual_run);
			uint16_t val = std::get<1>(manual_run);
			data.push_back(timestamp & 0xFF);
			data.push_back((timestamp >> 8) & 0xFF);
			data.push_back((timestamp >> 16) & 0xFF);
			data.push_back((timestamp >> 24) & 0xFF);
			data.push_back(val & 0xFF);
			data.push_back((val >> 8) & 0xFF);
		}
	}

	//this->header.content_length = data.size();
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool Feedback::fromBinary(const std::vector<uint8_t> &data){

	if(isValidData(data))
	{
		if(getSizefromBinary(data) == data.size())
		{
			if(this->header.fromBinary(data))
			{
				this->manual_time=data[HEADER_SIZE] | (data[HEADER_SIZE + 1] << 8);
				this->before_time=data[HEADER_SIZE+2] | (data[HEADER_SIZE + 3] << 8);
				this->after_time=data[HEADER_SIZE+4] | (data[HEADER_SIZE + 5] << 8);
				int prgm_count = data[HEADER_SIZE+6];
				int zone_count = data[HEADER_SIZE+7];

				this->zone_runs.resize(prgm_count);
				if(prgm_count){
					for(int i = 0; i < prgm_count; i++){
						zone_runs[i].resize(zone_count);
					}
				}
				int offset = HEADER_SIZE+8;
				for(int i = 0; i < prgm_count; i++){
					for(int j = 0; j < zone_count; j++){
						zone_runs[i][j].voltage = data[offset + (i * zone_count + j)*6]/5.0f;
						zone_runs[i][j].xfmr_voltage = data[offset + (i * zone_count + j)*6 + 1]/5.0f;
						zone_runs[i][j].current = data[offset + (i * zone_count + j)*6+ 2]/100.0f;
						zone_runs[i][j].flow = data[offset + (i * zone_count + j)*6 + 3];
						zone_runs[i][j].duration = data[offset + (i * zone_count + j)*6 + 4];
						zone_runs[i][j].volt_duration = data[offset + (i * zone_count + j)*6 + 5];
					}
				}
				offset += prgm_count * zone_count * 6;
				int manual_count = data[offset];
				offset++;
				manual_runs.resize(manual_count);
				for(int i = 0 ; i < manual_count; i++){
					int timestamp = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
					uint16_t val = data[offset+4] | (data[offset+5] << 8);
					offset += 6;
					manual_runs[i] = std::make_tuple(timestamp, val);
				}
				std::cout << "SIZE: ================================" << data.size() << std::endl;
				return true;
			} else return false;
		} else return false;
	} else return false;
}

Firmware::Firmware()
{
	this->md5_64 = 0L;
}

std::vector<uint8_t> Firmware::toBinary(const char* firmware, int size) const
{
	std::vector<uint8_t> data(this->header.toBinary());
	if(firmware && size > 0){
		for(int i = 0; i < 8; i++){
			data.push_back((md5_64 >> (i*8))&0xff);
		}
		data.insert(data.end(),firmware, firmware + size);
	}
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}


bool Firmware::fromBinary(const std::vector<uint8_t> &data, char* &firmware, int &size)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size()){
			memcpy(&this->md5_64, &data[HEADER_SIZE], 8);
			size = data.size() - HEADER_SIZE - 8;
			if(firmware){
				delete[] firmware;
				firmware = NULL;
			}
			firmware = new char[size];
			std::copy(data.begin()+HEADER_SIZE+8,data.end(), firmware);
			return true;
		}
	}
}

FlowFeedback::FlowFeedback(){
	this->header.type = FLOW;
}

FlowFeedback::FlowFeedback(const Header &header, const std::vector<std::tuple<int32_t, float>> &samples){
	this->header = header;
	this->header.type = FLOW;
	this->samples = samples;
}

std::vector<uint8_t> FlowFeedback::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	float max_flow = 0.0f;
	uint16_t count = 0;
	for(auto sample : this->samples){
		float flow = std::get<1>(sample);
		int32_t t = std::get<0>(sample) - this->header.timestamp;
		if(t >= 0 and t < 86400){
			count++;
			if(flow > max_flow) max_flow = flow;
		}
	}
	int max_flow_int = *((int*)(&max_flow));
	data.push_back(max_flow_int & 0xFF);
	data.push_back((max_flow_int >> 8) & 0xFF);
	data.push_back((max_flow_int >> 16) & 0xFF);
	data.push_back((max_flow_int >> 24) & 0xFF);
	data.push_back(count&0xFF);
	data.push_back((count>>8)&0xFF);

	for(auto sample : this->samples){
		float flow = std::get<1>(sample);
		int32_t t = std::get<0>(sample) - this->header.timestamp;
		if(t >= 0 and t < 86400){
			uint32_t val = t & 0x1FFFF;
			if(max_flow > 0.0f){
				uint32_t normalized_flow = (uint32_t)((flow / max_flow) * 0x7FFF + 0.5f);
				val |= normalized_flow << 17;
			}

			data.push_back(val & 0xFF);
			data.push_back((val >> 8) & 0xFF);
			data.push_back((val >> 16) & 0xFF);
			data.push_back((val >> 24) & 0xFF);
		}
	}

	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool FlowFeedback::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size()){
			if(this->header.fromBinary(data))
			{
				this->samples.clear();
				float max_flow;
				*((int *)(&max_flow)) = data[HEADER_SIZE] | (data[HEADER_SIZE+1] << 8) | (data[HEADER_SIZE+2] << 16) | (data[HEADER_SIZE+3] << 24);
				int count = data[HEADER_SIZE+4] | (data[HEADER_SIZE+5] << 8);
				int offset = HEADER_SIZE + 6;
				for(int i = 0; i<count; i++){
					int val = data[offset++] | (data[offset++] << 8) | (data[offset++] << 16) | (data[offset++] << 24);
					int timestamp = val & 0x1FFFF + this->header.timestamp;
					float flow = ((val >> 17)&0x7FFF) * max_flow;
					this->samples.push_back(std::make_tuple(timestamp, flow));
				}
				return true;
			}
		}
	}
}

AlertFeedback::AlertFeedback(){

}

AlertFeedback::AlertFeedback(const Header &header, const std::vector<std::tuple<int32_t, char, std::string>> &alerts){
	this->header = header;
	this->header.type = ALERT;
	this->alerts = alerts;
}

std::vector<uint8_t> AlertFeedback::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	data.push_back(this->alerts.size()&0xFF);
	data.push_back((this->alerts.size()>>8)&0xFF);
	for(auto alert : this->alerts){
		int32_t t = std::get<0>(alert);
		char type = std::get<1>(alert);
		std::string s = std::get<2>(alert);
		//std::cout << t << " " << type << " " << s << std::endl;
		data.push_back(t & 0xFF);
		data.push_back((t >> 8) & 0xFF);
		data.push_back((t >> 16) & 0xFF);
		data.push_back((t >> 24) & 0xFF);
		data.push_back(type);
		data.push_back(s.length());
		std::copy(s.begin(), s.end(), std::back_inserter(data));
	}
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool AlertFeedback::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size()){
			if(this->header.fromBinary(data))
			{
				this->alerts.clear();
				uint16_t size = data[HEADER_SIZE] | (data[HEADER_SIZE + 1] << 8);
				int offset = HEADER_SIZE + 2;
				for(int i = 0; i < size; i++){
					int timestamp = data[offset++] | (data[offset++] << 8) | (data[offset++] << 16) | (data[offset++] << 24);
					char type = data[offset++];
					int str_len = data[offset++];
					std::string str(data.begin() + offset, data.begin() + offset + str_len);
					offset += str_len;
					this->alerts.push_back(std::make_tuple(timestamp, type, str));

				}
				return true;
			}
		}
	}
}

CalibrationSetup::CalibrationSetup(){

}

CalibrationSetup::CalibrationSetup(const Header &header, std::vector<uint8_t> zones, uint8_t min_time, uint8_t max_time){
	this->header = header;
	this->header.type=FLOW_CAL;
	this->zones = zones;
	this->min_sample_time = min_time;
	this->max_sample_time = max_time;
}

bool CalibrationSetup::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size() && data.size() == HEADER_SIZE+7){
			if(this->header.fromBinary(data))
			{
				this->zones.clear();
				this->min_sample_time = data[HEADER_SIZE];
				this->max_sample_time = data[HEADER_SIZE + 1];
				uint64_t zone_mask = 0;
				memcpy(&zone_mask,&data[HEADER_SIZE+2],5);
				for(int i = 0; i<40; i++){
					if(zone_mask & (1<<i)){
						this->zones.push_back(i);
					}
				}
				return true;
			}
		}
	}
}

std::vector<uint8_t> CalibrationSetup::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());

	data.push_back(this->min_sample_time);
	data.push_back(this->max_sample_time);

	uint64_t zone_mask = 0;
	for(int zone : this->zones){
		zone_mask |= 1L << zone;
	}

	data.push_back(zone_mask & 0xFF);
	data.push_back((zone_mask >> 8) & 0xFF);
	data.push_back((zone_mask >> 16) & 0xFF);
	data.push_back((zone_mask >> 24) & 0xFF);
	data.push_back((zone_mask >> 32) & 0xFF);

	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}



FlowConfiguration::FlowConfiguration()
{
	this->ID = FLOW_CONFIG;
	this->offset = 0;
	this->K = 0;
	this->flow_thr_high = 0;
	this->flow_thr_low = 0;
	this->flow_thr_min = 0;
	this->flow_interval = 0;
	this->flow_count_thr = 0;
}

bool FlowConfiguration::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size()){
			if(this->header.fromBinary(data))
			{
				this->ID = data[HEADER_SIZE];
				memcpy(&this->offset, &data[HEADER_SIZE + 1], 4);
				memcpy(&this->K, &data[HEADER_SIZE + 5], 4);
				this->flow_thr_high = data[HEADER_SIZE+9];
				this->flow_thr_low = data[HEADER_SIZE+10];
				memcpy(&this->flow_thr_min, &data[HEADER_SIZE+11], 4);
				this->flow_interval = data[HEADER_SIZE+15] | (data[HEADER_SIZE+16] << 8);
				this->flow_count_thr = data[HEADER_SIZE + 17];
				return true;
			}
		}
	}
}

std::vector<uint8_t> FlowConfiguration::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	data.push_back(this->ID);
	int val = *((int*)(&this->offset));
	data.push_back(val & 0xFF);
	data.push_back((val >> 8) & 0xFF);
	data.push_back((val >> 16) & 0xFF);
	data.push_back((val >> 24) & 0xFF);
	val = *((int*)(&this->K));
	data.push_back(val & 0xFF);
	data.push_back((val >> 8) & 0xFF);
	data.push_back((val >> 16) & 0xFF);
	data.push_back((val >> 24) & 0xFF);
	data.push_back(this->flow_thr_high);
	data.push_back(this->flow_thr_low);
	val = *((int*)(&this->flow_thr_min));
	data.push_back(val & 0xFF);
	data.push_back((val >> 8) & 0xFF);
	data.push_back((val >> 16) & 0xFF);
	data.push_back((val >> 24) & 0xFF);
	data.push_back(this->flow_interval & 0xFF);
	data.push_back((this->flow_interval >> 8) & 0xFF);
	data.push_back(this->flow_count_thr);

	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

CalibrationResult::CalibrationResult()
{
	this->header.type=FLOW_RESULT;
}
std::vector<uint8_t> CalibrationResult::toBinary() const
{
	std::vector<uint8_t> data(this->header.toBinary());
	data.push_back(this->flow_values.size()&0xFF);
	for(auto value : this->flow_values){
		uint8_t zone = std::get<0>(value);
		float avg = std::get<1>(value);
		float dev = std::get<2>(value);
		data.push_back(zone);
		uint8_t* arr;
		arr = reinterpret_cast<uint8_t*>(&avg);
		std::copy(arr,arr+4,std::back_inserter(data));
		arr = reinterpret_cast<uint8_t*>(&dev);
		std::copy(arr,arr+4,std::back_inserter(data));
	}
	putSizeIntoData(data);
	calculateCRC(data);
	return data;
}

bool CalibrationResult::fromBinary(const std::vector<uint8_t> &data)
{
	if(isValidData(data)){
		if(getSizefromBinary(data) == data.size()){
			if(this->header.fromBinary(data))
			{
				int len = data[HEADER_SIZE];
				int offset = HEADER_SIZE+1;
				for(int i = 0; i < len; i++){
					uint8_t zone = data[offset];
					float avg;
					float dev;
					memcpy(&avg, &data[offset+1], 4);
					memcpy(&dev, &data[offset+5], 4);
					this->flow_values.push_back(std::make_tuple(zone, avg, dev));
					offset += 9;
				}
				return true;
			}
		}
	}
}

}