#ifndef  __MODEM_H__
#define  __MODEM_H__

#include <string>
#include <stdint.h>
#include <time.h>


#include "failure_exception.h"

#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <wiringPi.h>
#include <thread>
#include "modem_driver.h"

//#include "modem_driver.h"
#include "constants.h"

struct modem_device_t{
	std::string manufacturer;
	std::string softwareVersion;
	std::string model;
};

static inline void delay_ms(int d){
    std::this_thread::sleep_for(std::chrono::milliseconds(d));
}


static inline boost::posix_time::ptime ptime (void)
{
	return boost::posix_time::microsec_clock::local_time();
}



#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//#if defined DEBUG_MODEM
#define TRACE_EN
#define TRACE_INFO(...)    do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
#define TRACE_DEBUG(...)   do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
//#else
//#define TRACE_INFO(...)
//#define TRACE_DEBUG(...)
//#endif

template<typename T>
void TRACE(T arg) {
#if defined TRACE_EN
	std::cout << arg << std::endl;
#endif
}

template<typename T, typename... Args>
void TRACE(T arg, Args... args)
{
#if defined TRACE_EN
	std::cout << arg;
#endif
    TRACE(args...);
}


struct modem_reply_t{
	std::string host;
	uint16_t port;
	std::string requestLine;
	std::string request_headers;
	std::string request_messageBody;
	std::string response_headers;
	std::string response_messageBody;
	int statusCode;
	std::string reasonPhrase;
};

struct modem_info_t{
	std::string phone;
	int rssi;
	std::string imei;
	std::string manufacturer;
	std::string softwareVersion;
	std::string model;
};

struct modem_signal_t{
	std::string rssi;
	std::string ber;
};

class ModemDriver; // forward declaration of ModemDriver

class Modem {
protected:
	ModemDriver *modem;
	int fail1, fail2, fail3, fail4, fail5;
	uint8_t  errors;

public:
	Modem();
	~Modem();
	enum ErrType {NONE = 0, BOOT_FAIL, SERIAL_PORT_FAIL, REPLY_TIMEOUT, URC_TIMEOUT, PDP_QUERY_ERROR, PDP_DEFINE_ERROR, SICA_FAIL, INET_FAIL, INET_QUERY_FAIL, SIST_FAIL, SOCKET_FAIL, HTTP_FAIL, INF3c0, NO_SIGNAL, UNKNOWN};
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
	
	virtual bool SendCmd(const std::string &cmd, std::string &data, boost::posix_time::time_duration timeout = boost::posix_time::seconds(1), bool end_on_ok = true);

	virtual bool connect(){
		return false;
	}

	virtual bool sendRequest(const std::string &headers, modem_reply_t &reply);

	/*ErrType HttpRequest(modem_reply_t &reply,
			std::string  host,
			uint16_t     port,
			std::string  requestLine,
			std::string  headers);

	ErrType HttpRequest(modem_reply_t &reply,
			std::string  host,
			uint16_t     port,
			std::string  requestLine,
			std::string  headers,
			const std::string &messageBody);*/
};

#endif
