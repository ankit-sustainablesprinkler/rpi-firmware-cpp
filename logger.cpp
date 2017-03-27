#include <lcm/lcm-cpp.hpp>
#include <iostream>
#include <fstream>
#include <time.h>
#include "Message.hpp"
#include "constants.h"
#include <stdio.h>

using namespace std;

void logEvent(const string &s);

int main(int argc, char **argv)
{
	Message message;
	lcm::LCM lcm;
	
	string new_name = string(FILE_PREFIX) + "system_log_" + to_string(time(nullptr)) + ".txt";
	
	rename(SYSTEM_LOG, new_name.c_str());
	
	while(!cin.eof())
	{
		getline(cin, message.msg);
		logEvent(message.msg);
		lcm.publish("LOGGER", &message);
		cout << "logged " << message.msg << endl;
	}
	
	return 0;
}

void logEvent(const string &s)
{
	ofstream file;
	file.open(SYSTEM_LOG, ofstream::out | ofstream::app);
	if(file.is_open()){
		file << time(nullptr) << ", " << s << endl;
		file.close();
	}
}