#include "bin_protocol.h"
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

Heartbeat::Heartbeat(Header header, int up_time, int schedule_id, int config_id, int tempurature, int signal, std::string state, std::string extra_content)
{
	this->header = header;
	this->up_time = up_time;
	this->schedule_id = schedule_id;
	this->config_id = config_id;
	this->temperature = temperature;
	this->signal = signal;
	this->state = state;
	this->extra_content = extra_content;
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
				this->temperature = data[HEADER_SIZE+6];
				this->signal = data[HEADER_SIZE+7];
				this->state = (data[HEADER_SIZE+8] & 0x01) == 0x01 ? "close" : "open";
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
			int size = -((-prog_count*zone_count)/8); //round up
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
				int zone_data_length = -((-program_count*zone_count)/8);
				this->effective_date = data[HEADER_SIZE + 3] | (data[HEADER_SIZE+4] << 8) | (data[HEADER_SIZE+5] << 16) | (data[HEADER_SIZE+6] << 24);
				this->days = data[HEADER_SIZE + 7] & 0x7F;
				this->is_shift = (data[HEADER_SIZE+7]&0x80) == 0 ? false : true;
				int offset = HEADER_SIZE + 8;
				int i = 0;
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

Config::Config()
{	
	this->ID = 0;
	this->manual_start_time = 0;
	this->manual_end_time = 0;
	this->heartbeat_period = 0;
	this->system_time_offset = 0;
	this->station_delay = 0;
	this->remain_closed = 0;
}

Config::Config(Header header, int ID, int manual_start_time, int manual_end_time, int heartbeat_period, int16_t system_time_offset,\
               int station_delay, bool remain_closed)
{
	this->header = header;
	this->ID = ID;
	this->manual_start_time = manual_start_time;
	this->manual_end_time = manual_end_time;
	this->heartbeat_period = heartbeat_period;
	this->system_time_offset = system_time_offset;
	this->station_delay = station_delay;
	this->remain_closed = remain_closed;
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
	data.push_back((this->station_delay >> 8) & 0x7F | (this->remain_closed ? 0x80 : 0x00));

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
				/*if(this->system_time_offset > 0x7FFF)
				{
					this->system_time_offset -= 0x10000  #since python does not have a int16_t (or any explicit type), we have to manualy calculate it like a savage
				}*/
				this->station_delay = (data[HEADER_SIZE+9] | (data[HEADER_SIZE+10] << 8)) & 0x7FFF; // remove most significant bit because it is for the remain_closed
				this->remain_closed = (data[HEADER_SIZE + 10] & 0x80) != 0x00;
				return true;
			} else return false;
		} else return false;
	} else return false;
}
/*
class MessageFlow:
	def __init__(self, header=MessageHeader(), averages=[]):
		this->header = header
		this->averages = averages
	def toBinary(self):
		data = bytearray()
		data.extend(this->header.toBinary())

		zone_count = len(this->averages)
		program_count = len(this->averages)
		if program_count > 0:
			zone_count = len(this->averages[0])
			if zone_count > 0:
				data.append(zone_count & 0xFF)
				data.append(program_count & 0xFF)

				for program in this->averages:
					for zone in program:
						data.append(len(zone))
				for program in this->averages:
					for zone in program:
						for sample in zone:
							data.append(sample & 0xFF)
							data.append((sample >> 8) & 0xFF)

		this->header.content_length = len(data)
		this->header.putSizeIntoData(data)
		calculateCRC(data, this->header)
		return data

	def fromBinary(self, data):
		if isValidData(data):
			if getSizefromBinary(data) == len(data):
				this->header = MessageHeader()
				if this->header.fromBinary(data):
					sample_count = 0
					sample_sizes = []
					zone_count = data[HEADER_SIZE]
					program_count = data[HEADER_SIZE + 1]
					offset = HEADER_SIZE + 2
					i=0
					while(i < program_count):
						j = 0
						sample_size_row = []
						while(j < zone_count):
							sample_size_row.append(data[offset+j+i*zone_count])
							j+=1
						sample_sizes.append(sample_size_row)
						i+=1
					for row in sample_sizes:
						for size in row:
							sample_count += size

					if(HEADER_SIZE + 2 + zone_count*program_count + sample_count*2 == this->header.content_length):
						this->averages = []
						offset = HEADER_SIZE + 2 + zone_count*program_count
						for row in sample_sizes:
							program = []
							for size in row:
								i = 0
								zone = []
								while(i < size):
									zone.append(data[offset + i*2] | (data[offset + i*2 + 1] << 8))
									i+=1
								program.append(zone)
								offset += size*2
							this->averages.append(program)
						return True
					else: return False
				else: return False
			else: return False
		else: return False

class MessageFlowCalibrate:
	def __init__(self, header=MessageHeader(), zone=0):
		this->zone = zone
		this->header = header
	def toBinary(self):
		data = bytearray()
		data.extend(this->header.toBinary())

		data.append(this->zone & 0xFF)
		this->header.content_length = len(data)
		this->header.putSizeIntoData(data)
		calculateCRC(data, this->header)
		return data
	def fromBinary(self, data):
		if isValidData(data):
			if getSizefromBinary(data) == len(data):
				this->header = MessageHeader()
				if this->header.fromBinary(data):
					this->zone = data[HEADER_SIZE]
					return True
				else: return False
			else: return False
		else: return False
		*/
}