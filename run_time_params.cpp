#include "run_time_params.h"
#include <regex>
#include <sstream>

using namespace std;

RunTimeParams::RunTimeParams()
{
	modemEnable = true;
	shutdownInterval = 7;
}

ParamErrType RunTimeParams::setValue(const string &key, const string &value)
{
	ParamErrType result = None;
	if(key == "modem"){
		if(value == "on") modemEnable = true;
		else if(value == "off") modemEnable = false;
		else result = ValueError;
	} else if(key == "shutdown_interval"){
		try{
			int interval = stoi(value);
			if(interval > 0) shutdownInterval = interval;
		} catch (...){
			result = ValueError;
		}
	} else {
		result = KeyError;
	}
	return result;
}

ParamErrType RunTimeParams::setValues(const string &m, string &err_str)
{

	ParamErrType result = None;
	std::stringstream ss(m);
	std::string line;
	int i = 0;
	while(std::getline(ss,line,'\n')){
		line = regex_replace(line, regex("^ +| +$|( ) +"), "$1");
		string::size_type pos;
		pos=line.find(' ',0);
		if(line[0] != '#'){
			err_str = line;
			if(pos != string::npos){
				string key = line.substr(0,pos);
				string value = line.substr(pos+1);
				result = setValue(key,value);
			} else {
				result = ValueError;
			}
		}
		if(result) break;
	}
	return result;
}

bool RunTimeParams::isModemEnabled()
{
    return modemEnable;
}


int RunTimeParams::getRunTimeInterval()
{
	return shutdownInterval;
}
