#ifndef  __MODEM_H__
#define  __MODEM_H__

#include <string>
#include <stdint.h>
#include <time.h>

//#include "modem_driver.h"
#include "constants.h"

struct modem_device_t{
	std::string manufacturer;
	std::string softwareVersion;
	std::string model;
};

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
private:
	ModemDriver *modem;
	void  set_error(void);
	int fail0, fail1, fail2, fail3, fial0;

public:
	Modem();
	~Modem();
	enum ErrType {NONE = 0, BOOT_FAIL, SERIAL_PORT_FAIL, REPLY_TIMEOUT, URC_TIMEOUT, PDP_QUERY_ERROR, PDP_DEFINE_ERROR, SICA_FAIL, INET_FAIL, INET_QUERY_FAIL, SIST_FAIL, SOCKET_FAIL, HTTP_FAIL, INF3c0, NO_SIGNAL, UNKNOWN};
	void init();
	bool Open(const std::string &device, int baudrate);
	bool waitForReady(time_t timeout=120);

	modem_device_t Device();
	std::string Phone();
	std::string IMEI();
	int Signal();
	bool getInfo(modem_info_t &info);
	bool PowerOn();
	bool PowerOff();
	bool Reset();
	void hardReset();

	//modem's fail routine
	bool ohShit(int fail_state);

	bool sendRequest(const std::string &headers, modem_reply_t &reply);

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
