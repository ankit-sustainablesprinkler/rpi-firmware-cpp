#ifndef  __MODEM_DRIVER_H__
#define  __MODEM_DRIVER_H__

#include  <thread>
#include  <boost/date_time/posix_time/posix_time.hpp>
#include  <boost/date_time/posix_time/posix_time_duration.hpp>
#include <string>

#define NO_CARRIER "\020\004\r\nNO CARRIER\r\n"
#define MODEM_CHUNK_SIZE 2048
#define BUFFER_SIZE 1048576

//#include  "serial_driver.h"

class SerialDriver;

class ModemDriver {
private:

	int reply_failures;
	int restart_count;
	char *reply;
	//char c_buffer[BUFFER_SIZE];
	std::vector<char> buffer;

	int  read_line(int index = 0);
	bool read_data (boost::posix_time::time_duration timeout, size_t &len);
	bool read_urc (const std::string &urc, boost::posix_time::time_duration timeout);
	bool is_final_result(const char* reply, bool end_on_ok);
	bool is_unsolicited(const char* reply);
	void process_reply(bool end_on_ok, boost::posix_time::time_duration timeout = boost::posix_time::seconds(1));
	bool process_data(boost::posix_time::time_duration timeout = boost::posix_time::seconds(1));
	bool process_urc (const std::string &urc, boost::posix_time::time_duration timeout);
	void flush(void);

	std::string data;
	std::string response;

	SerialDriver *port;

public:
	ModemDriver();
	~ModemDriver();

	bool  SendData(const std::string &data);
	bool  RecvData(std::string &data, boost::posix_time::time_duration timeout);
	bool  WaitURC(const std::string &urc, std::string &data, boost::posix_time::time_duration timeout);
	bool  SendCmd(const std::string &cmd, std::string &data, boost::posix_time::time_duration timeout = boost::posix_time::seconds(1), bool end_on_ok = true);
	
	void exitDataMode();

	bool  Open(const std::string &device, int baudrate);
	bool  IsOpen();
	void  Close();
};

#endif
