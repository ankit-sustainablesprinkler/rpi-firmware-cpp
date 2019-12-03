#ifndef RUN_TIME_PARAMS_H
#define RUN_TIME_PARAMS_H

#include <string>

using namespace std;

enum ParamErrType{None, KeyError, ValueError};

class RunTimeParams
{
public:
	RunTimeParams();
	ParamErrType setValue(const string &key, const string &value);
	ParamErrType setValues(const string &m, string &err_str);

	bool isModemEnabled();
	int getRunTimeInterval();
private:

	bool modemEnable;
	int shutdownInterval;

};

#endif // RUN_TIME_PARAMS_H
