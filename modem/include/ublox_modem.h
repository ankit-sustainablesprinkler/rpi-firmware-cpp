#ifndef _UBLOX_H_
#define _UBLOX_H_

#include "modem.h"

class SARA_R410M: public Modem{

public:
	
	SARA_R410M();
	~SARA_R410M();

	virtual void init();
	virtual bool Open(const std::string &device, int baudrate);
	virtual bool waitForReady(time_t timeout=20);

	virtual modem_device_t Device();
	virtual std::string Phone();
	virtual std::string IMEI();
	virtual int Signal();
	virtual bool getInfo(modem_info_t &info);
	virtual bool PowerOn();
	virtual bool PowerOff();
	virtual bool Reset();
	virtual bool hardReset();

	virtual bool connect();

	virtual bool sendRequest(const std::string &headers, modem_reply_t &reply);

	virtual void enableEcho();
	virtual void dissableEcho();
protected: 

	bool enable_echo;

};


#endif