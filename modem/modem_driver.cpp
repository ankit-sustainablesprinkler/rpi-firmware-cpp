#include  "modem_driver.h"
#include  "serial_driver.h"
#include  <wiringPi.h>
#include <iostream>

using namespace std;
using namespace boost;


#define REPLY_FAILURES_RESTART 0
#define REPLY_FAILURES_REBOOT  3
#define COMMAND_TIMEOUT_MS 1000

#define STARTS_WITH(a, b) ( strncmp((a), (b), strlen(b)) == 0)

//#if defined DEBUG_MODEM
#define TRACE_EN
#define TRACE_INFO(...)    do { auto _ts = time().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
#define TRACE_DEBUG(...)   do { auto _ts = time().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
//#else
//#define TRACE_INFO(...)
//#define TRACE_DEBUG(...)
//#endif


static inline posix_time::ptime time (void)
{
	return posix_time::microsec_clock::local_time();
}

template<typename T>
void TRACE(T arg) {
#ifdef TRACE_EN
	std::cout << arg << std::endl;
#endif
}

template<typename T, typename... Args>
void TRACE(T arg, Args... args)
{
#ifdef TRACE_EN
	std::cout << arg;
#endif
    TRACE(args...);
}


ModemDriver::ModemDriver()
{
	port = new SerialDriver();
	restart_count = 0;
	reply_failures = 0;
	//memset(reply, 0, BUFFER_SIZE);
	reply = new char[BUFFER_SIZE];
}

ModemDriver::~ModemDriver()
{
	delete[] reply;
	delete port;
	port = nullptr;
}

bool ModemDriver::Open(const std::string &device, int baudrate)
{
	return port->open(device, baudrate);
}

void ModemDriver::Close()
{
	return port->close();
}

bool ModemDriver::IsOpen(void)
{
	return port->is_open();
}

// update and return index to reply buffer
int ModemDriver::read_line(int index)
{
  char inChar = 0; // Where to store the character read
  auto last = time();

  do {
	  if (buffer.empty()) {
		  char chunk[2048];
		  streamsize len = 0;

		  try {
			  len = port->read(chunk, sizeof(chunk));
		  } catch (const TimeoutException &e) {

		  }
		  if (len) buffer.insert(buffer.end(), chunk, chunk + len);
	  }
	  if (!buffer.empty()) {
		  inChar = buffer[0];
		  buffer.erase(buffer.begin());

		  last = time();
		  if(index < (BUFFER_SIZE - 1)) { // One less than the size of the array
			reply[index] = inChar; // Store it
			index++; // Increment where to write next

			if(index == (BUFFER_SIZE - 1) || (inChar == '\n')) { //some data still available, keep it in serial buffer
			  break;
			}
		  }
	  }
  } while((time() - last) < posix_time::milliseconds(50)); // allow some inter-character delay

  reply[index] = '\0'; // Null terminate the string
  return index;
}

bool ModemDriver::read_data (posix_time::time_duration timeout, size_t &len)
{
	const char no_carrier[] = { "\020\004\r\nNO CARRIER\r\n" };
	int wr_ix = 0;
	int rd_ix = 0;
	bool complete = false;
	auto start = time();

	do {
		if (buffer.empty()) {
			char chunk[2048];
			streamsize read = 0;

			try {
				read = port->read(chunk, sizeof(chunk));
			} catch (const TimeoutException &e) {

			}
			if (read) buffer.insert(buffer.end(), chunk, chunk + read);
		}
		if (!buffer.empty() && (wr_ix < (BUFFER_SIZE - 1))) {
			reply[wr_ix++] = buffer[0];
			buffer.erase(buffer.begin());
		}
		if (wr_ix >= rd_ix + 16) {
			if (memcmp(&reply[rd_ix], no_carrier, 16) == 0) {
				complete = true;
				break;
			}
			rd_ix++;
		}
	} while ((time() - start) < timeout);

	reply[wr_ix] = '\0'; // Null terminate the string

	if (complete)
		len = rd_ix;
	else
		len = wr_ix;

	return complete;
}
/*
bool ModemDriver::read_urc (const std::string &urc, posix_time::time_duration timeout)
{
	int wr_ix = 0;
	int rd_ix = 0;
	int urc_ix = 0;
	bool complete = false;
	bool found = false;
	auto start = time();
	char chunk[MODEM_CHUNK_SIZE];

	do {
		//if (buffer.empty()) {
		streamsize read = 0;

		try {
			read = port->read(chunk, sizeof(chunk));
			//std::cout << chunk << std::endl;
			
		} catch (const TimeoutException &e) {

		}
			//if (read) buffer.insert(buffer.end(), chunk, chunk + read);
		//}
		if (!buffer.empty() && (wr_ix < (BUFFER_SIZE - 1))) {
			reply[wr_ix++] = buffer[0];
			buffer.erase(buffer.begin());
		}
		if (!found) {
			if (wr_ix >= rd_ix + urc.size()) {
				if (urc.compare(&reply[rd_ix]) == 0) {
					found = true;
					urc_ix = rd_ix;
				}
				rd_ix++;
			}
		} else {
			if ((reply[rd_ix - 2] == '\r') &&
				(reply[rd_ix - 1] == '\n')) {
				complete = true;
				break;
			}
			rd_ix++;
		}
	} while ((time() - start) < timeout);

	if (complete)
		data = string(&reply[urc_ix], rd_ix - urc_ix);
	else
		data = "";

	return complete;
}*/


bool ModemDriver::read_urc (const std::string &urc, posix_time::time_duration timeout)
{
	int wr_ix = 0;
	int rd_ix = 0;
	int urc_ix = 0;
	bool complete = false;
	bool found = false;
	auto start = time();

	do {
		if (buffer.empty()) {
			char chunk[MODEM_CHUNK_SIZE] = {0};
			streamsize read = 0;

			try {
				read = port->read(chunk, sizeof(chunk));
		//		std::cout << chunk << std::endl;
			} catch (const TimeoutException &e) {

			}
			if (read) buffer.insert(buffer.end(), chunk, chunk + read);
		}
		if (!buffer.empty() && (wr_ix < (BUFFER_SIZE - 1))) {
			reply[wr_ix++] = buffer[0];
			buffer.erase(buffer.begin());
		}
		if (!found) {
			if (wr_ix >= rd_ix + urc.size()) {
		//		std::cout << "Char: " << reply[rd_ix] << " " << urc.c_str() << " " << urc.length() << std::endl;
				//if (urc.compare(&reply[rd_ix]) == 0) {  // This does not work.
				if(!strncmp(reply+rd_ix, urc.c_str(), urc.length())){  //but c 
					found = true;
					urc_ix = rd_ix;
				}
				rd_ix++;
			}
		} else {
			if ((reply[rd_ix - 2] == '\r') &&
				(reply[rd_ix - 1] == '\n')) {
				complete = true;
				break;
			}
			rd_ix++;
		}
	} while ((time() - start) < timeout);

	if (complete)
		data = string(&reply[urc_ix], rd_ix - urc_ix);
	else
		data = "";

	return complete;
}

void ModemDriver::flush(void)
{
	bool printed = false;
	char chunk[2048];
	streamsize len;

	if (buffer.size() > 0) {
		if ((buffer.size() != 1) || (buffer[0] != '+')) {
			
#if defined DEBUG_MODEM
			cout << "Flush: \"";
			std::copy(buffer.begin(), buffer.end(), std::ostream_iterator<char>(std::cout));
			cout << "\"";
#endif
		}
	}

	buffer.clear();

	do {
		try {
			len = port->read(chunk, sizeof(chunk));
			if (len > 0) {
				if ((len != 2) || (chunk[0] != 'A') || (chunk[1] != 'T')) {
					if (!printed)
						
#if defined DEBUG_MODEM
						cout << "Flush: \"";
#endif

					printed = true;

#if defined DEBUG_MODEM
					cout << chunk;
#endif
				}
			}
		} catch (const TimeoutException &e) {
			len = 0;
		}
	} while (len > 0);

#if defined DEBUG_MODEM
	if (printed)
		cout << "\"";
#endif
}


void ModemDriver::process_reply(bool end_on_ok, posix_time::time_duration timeout)
{
  auto lastXfer = time();

  reply[0] = '\0';
  int ret = 0;

  int rd_ptr = 0, wr_ptr = 0;

  do {
    wr_ptr = read_line(rd_ptr);

    if (wr_ptr > rd_ptr) {
      lastXfer = time();

      bool skip = is_unsolicited(&reply[rd_ptr]);
      if (skip) {
    	TRACE_DEBUG("Unsolicited: \"", &reply[rd_ptr], "\"");

    	reply[rd_ptr] = '\0';
        continue;
      }

      if (is_final_result(&reply[rd_ptr], end_on_ok)) {
        ret = 1;
        break;
      }

      rd_ptr = wr_ptr;

    } else if ((time() - lastXfer) > timeout) {
        break;
    } else {
    	std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  } while(wr_ptr < (BUFFER_SIZE - 1));

  if (ret == 0) {
	  TRACE_INFO("Modem reply timed out.\r\n");

      data = "";
      response = "";
  } else {

      if (rd_ptr > 2) {
          data = string(reply, rd_ptr - 2);
      } else {
    	  data = "";
      }

      int len = strlen(&reply[rd_ptr]);
      if (len > 0) {
    	  if (len > 2) {
    		  int eol = rd_ptr + len -2;
    		  if (strcmp(&reply[eol], "\r\n") == 0)
    			  len -= 2;
    	  }

    	  response = string(&reply[rd_ptr], len);
      } else {
    	  response = "";
      }
  }

  if (reply[0] == '\0') {
	reply_failures++;
  } else {
    reply_failures = 0;
    restart_count = 0;
  }

#if (REPLY_FAILURES_RESTART > 0)
  if (reply_failures > REPLY_FAILURES_RESTART) {
	restart_count++;
    if (restart_count > REPLY_FAILURES_REBOOT) {
    	restart_count = 0;

    	TRACE_INFO("Modem reply failures: ", reply_failures, " (rebooting)\r\n");

    	digitalWrite(28, HIGH);
    	sleep(2);
    	digitalWrite(28, LOW);
    } else {
    	TRACE_INFO("Modem reply failures: ", reply_failures, " (restarting)\r\n");
    }

   	reply_failures = 0;
	port->restart();
  }
#endif
}

bool  ModemDriver::process_data (posix_time::time_duration timeout)
{
	reply[0] = '\0';

	size_t len;
	bool valid = read_data(timeout, len);
	valid = len > 0;
	if (valid && (len > 0)) {
		data = string(reply, len);
	} else {
		data = "";
	}

	if (!valid) {
		if (len > 0) {
			TRACE_INFO("Modem reply timed out.\r\nPending Data {", reply, "}\r\n");
#if defined DEBUG_MODEM
			std::cout << "Pending Binary {";
#endif
			for (int ix = 0; ix < len; ix++) {
				if ((ix % 16) == 0)
#if defined DEBUG_MODEM
					std::cout << std::endl;
#endif

				printf("%02X ", reply[ix]);
			}
#if defined DEBUG_MODEM
			std::cout << "}" << std::endl;
#endif
		} else if (buffer.size() > 0) {
			TRACE_INFO("Modem reply timed out.\r\nI/O Buffer with ", buffer.size(), " bytes.\r\n");
		} else {
			TRACE_INFO("Modem reply timed out.\r\n");
		}
	}

	return valid;
}

bool  ModemDriver::process_urc (const std::string &urc, posix_time::time_duration timeout)
{
  reply[0] = '\0';

  bool valid = read_urc(urc, timeout);

  if (!valid)
	  TRACE_INFO("Modem URC timed out.\r\n");

  return valid;
}

bool ModemDriver::is_unsolicited(const char* reply)
{
  #define STARTS_WITH(a, b) ( strncmp((a), (b), strlen(b)) == 0)
  switch (reply[0]) {
    case '+':
      if(STARTS_WITH(&reply[1], "CEREG: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "CGEV: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "IMSSTATE: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "IMS: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "SQNREGISTEREDIMPU: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "CIEV: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "CTZV: ")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "CTZE: ")) {
        return true;
      }
      return false;
   case '^':
	  if(STARTS_WITH(&reply[1], "MODE: ")) {
		return true;
	  }
	  if(STARTS_WITH(&reply[1], "SYSSTART: ")) {
		return true;
	  }
	  if(STARTS_WITH(&reply[1], "SBC: ")) {
		return true;
	  }
	  if(STARTS_WITH(&reply[1], "SHUTDOWN")) {
		return true;
	  }
	  if(STARTS_WITH(&reply[1], "SCTM_A: ")) {
		return true;
	  }
	  if(STARTS_WITH(&reply[1], "SCTM_B: ")) {
		return true;
	  }
	  return false;
   case 'R':
	  if(strcmp(&reply[1], "ING\r\n") == 0) {
		return true;
	  }
      return false;
    default:
      return false;
  }
}


bool ModemDriver::is_final_result(const char* reply, bool end_on_ok)
{
  switch (reply[0]) {
    case '+':
      if(STARTS_WITH(&reply[1], "CME ERROR:")) {
        return true;
      }
      if(STARTS_WITH(&reply[1], "CMS ERROR:")) {
        return true;
      }
      return false;
    case 'B':
      if(strcmp(&reply[1], "USY\r\n") == 0) {
        return true;
      }
      return false;
    case 'C':
      if(strcmp(&reply[1], "ONNECT\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "ONNECT OK\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "ONNECT FAIL\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "LOSED\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "LOSE OK\r\n") == 0) {
        return true;
      }
      return false;
    case 'E':
      if(STARTS_WITH(&reply[1], "RROR")) {
        return true;
      }
      return false;
    case 'N':
      if(strcmp(&reply[1], "O ANSWER\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "O CARRIER\r\n") == 0) {
        return true;
      }
      if(strcmp(&reply[1], "O DIALTONE\r\n") == 0) {
        return true;
      }
      return false;
    case 'O':
      if(end_on_ok && strcmp(&reply[1], "K\r\n") == 0) {
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool ModemDriver::SendData(const std::string &data)
{
	flush();

	return port->write(data);
}

bool ModemDriver::SendCmd(const std::string &cmd, std::string &data, posix_time::time_duration timeout, bool end_on_ok)
{
	TRACE_DEBUG("AT", cmd);

	port->write("AT");

	flush();

	port->write(cmd);
	port->write("\n");

	process_reply(end_on_ok, timeout);

	data = this->data;
	if (data.find(cmd) == 0) {
		data = data.erase(0, cmd.size() + 3);
	}

	TRACE_DEBUG("Data {", data, "}");
	TRACE_DEBUG("Response {", response, "}");

	if ((response == "OK") ||
		(response == "CONNECT"))
		return true;
	else
		return false;
}

bool ModemDriver::RecvData(std::string &data, posix_time::time_duration timeout)
{
	bool valid = process_data(timeout);

	data = this->data;

	TRACE_DEBUG("Data {", data, "}");

	return valid;
}

bool ModemDriver::WaitURC (const std::string &urc, std::string &data, posix_time::time_duration timeout)
{
	bool valid = process_urc(urc, timeout);

	data = this->data;

	TRACE_DEBUG("URC {", data, "}");

	return valid;
}

void ModemDriver::exitDataMode()
{
	SendData("+++");
	//std::this_thread::sleep_for(std::chrono::milliseconds(50));
	//SendData("+");
//	SendData("+");
//	SendData("+");
//	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
