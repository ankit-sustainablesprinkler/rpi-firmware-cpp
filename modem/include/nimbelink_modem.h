#ifndef _NIMBELINK_H_
#define _NIMBELINK_H_

#include "modem.h"

class NL_SW_LTE_GELS3: public Modem{
protected:
	virtual void  set_error(void);

public:
	NL_SW_LTE_GELS3();
	~NL_SW_LTE_GELS3();

	virtual void init();
	virtual bool Open(const std::string &device, int baudrate);
	virtual bool waitForReady(time_t timeout=120);

	virtual modem_device_t Device();
	virtual std::string Phone();
	virtual std::string IMEI();
	virtual int Signal();
	virtual bool getInfo(modem_info_t &info);
	virtual bool PowerOn();
	virtual bool PowerOff();
	virtual bool Reset();
	virtual bool hardReset();

	//modem's fail routine
	virtual bool ohShit(int fail_state);

	virtual bool sendRequest(const std::string &headers, modem_reply_t &reply);

};

#endif