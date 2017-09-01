#include <iostream>
#include <fstream>
#include <sstream>

#include "functions.h"
#include "bin_protocol.h"
#include "base64.h"

using namespace std;
using namespace bin_protocol;

//bool runSchedule(const Schedule &schedule, const Config &config);

int main(int argc, char **argv)
{
	auto md5 = runCommand("md5sum s3-main.gz");
	cout << md5 << endl;
	stringstream converter(md5.substr(16));
	Firmware firmware;
	converter >> std::hex >> firmware.md5_64;
	ifstream f("s3-main.gz", ios::in | ios::ate | ios::binary);
	int size = f.tellg();
	char *firmware_data = new char[size];
	f.seekg(ios::beg);
	f.read(firmware_data, size);
	auto data = firmware.toBinary(firmware_data, size);
	char *new_firmware_data;
	int new_size;
	Firmware new_firmware;
	if(new_firmware.fromBinary(data, new_firmware_data, new_size)){
		cout << "here" << endl;
		stringstream ss;
		//ss << std::hex << new_firmware.md5lower;// << std::hex << new_firmware.md5upper;
		cout << firmware.md5_64 << endl;
		cout << new_firmware.md5_64 << endl;
	}
	cout << data.size() << endl;
}