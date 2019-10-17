#include  "ublox_modem.h"
#include "failure_exception.h"

#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <wiringPi.h>
#include <thread>
#include "modem_driver.h"

using boost::lexical_cast;
  
  
using namespace boost;

SARA_R410M::SARA_R410M() {
    this->modem = new ModemDriver();
    fail1 = fail2 = fail3 = fail4 = fail5 = 0;
}
SARA_R410M::~SARA_R410M() {
    delete this->modem;
    this->modem = nullptr;
}
  
void SARA_R410M::init()
{
    pinMode(PIN_MODEM_DTR, OUTPUT);                                     /* P1_37: SKY_DTR                                       */
    pinMode(PIN_MODEM_RTS, OUTPUT);                                     /* P1_36: SKY_RTS                                       */
    pinMode(PIN_MODEM_ON, OUTPUT);                                      /* P1_38: SKY_ON                                        */
  
    digitalWrite(PIN_MODEM_RTS, LOW);
    digitalWrite(PIN_MODEM_ON, HIGH);
}

  
bool SARA_R410M::Open(const std::string &device, int baudrate)
{
    return modem != nullptr && modem->Open(device, baudrate);
}
  
bool SARA_R410M::waitForReady(time_t timeout)
{
    time_t start_time = time(nullptr);
    std::string response;
    int state = 0;
    while (time(nullptr) - start_time < timeout) {
        if (modem->SendCmd("I", response)){
            return true;
  
		}
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return false;
}

modem_device_t SARA_R410M::Device()
{
    modem_device_t result;
    std::string  response;
    bool valid;
  
    if (modem == nullptr) {
        throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
    }
  
  
    if (!modem->IsOpen())
        throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));
  
    valid = modem->SendCmd("+CGMI", response);
    if (!valid) {
  
        throw(FailureException("Hardware Error", "Failed to retrieve modem manufacturer name.", "service_unavailable"));
    }
  
    errors = 0;
  
    if (response.length() > 2) {
        result.manufacturer = response.substr(0, response.length() - 2);
    }
    else {
        result.manufacturer = "<error>";
    }
  
    valid = modem->SendCmd("+CGMM", response);
    if (!valid)
        throw(FailureException("Hardware Error", "Failed to retrieve modem model name.", "service_unavailable"));
  
    if (response.length() > 2) {
        result.model = response.substr(0, response.length() - 2);
    }
    else {
        result.model = "<error>";
    }
  
    valid = modem->SendCmd("+CGMR", response);
    if (!valid)
        throw(FailureException("Hardware Error", "Failed to retrieve modem software version.", "service_unavailable"));
  
    if (response.length() > 2) {
        result.softwareVersion = response.substr(0, response.length() - 2);
    }
    else {
        result.softwareVersion = "<error>";
    }
  
    return result;
}
  
std::string SARA_R410M::Phone()
{
    std::string result;
    std::string response;
    if (modem == nullptr) {
        throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
    }
  
  
    if (!modem->IsOpen())
        throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));
  
  
    bool valid = modem->SendCmd("+CNUM", response);
    if (!valid) {
  
        throw(FailureException("Hardware Error", "Failed to retrieve modem phone number.",  "service_unavailable"));
    }
  
    errors = 0;
  
    if (response.length() > 0) {
        auto  start = response.find(',');
        if (start == std::string::npos) {
            result = "<invalid>";
        }
        else {
            std::string cnum = response.substr(start + 1);
            auto end = cnum.find(',');
            if (end == std::string::npos) {
                result = "<invalid>";
            }
            else {
                result = cnum.substr(0, end);
            }
        }
    }
    else {
        result = "<error>";
    }
    return result;
}
  
std::string SARA_R410M::IMEI()
{
    std::string result;
    std::string response;
  
  
    if (!modem->IsOpen())
        throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));
  
  
    bool valid = modem->SendCmd("+CGSN", response);
    if (!valid) {
  
        throw(FailureException("Hardware Error", "Failed to retrieve modem imei.", "service_unavailable"));
    }
  
    errors = 0;
  
    if (response.length() > 2) {
        result = response.substr(0, response.length() - 2);
    }
    else {
        result = "<error>";
    }
  
    return result;
}
  
int SARA_R410M::Signal()
{
    if (modem == nullptr) {
        throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
    }
  
    int result = 0;
    std::string response;
  
    if (!modem->IsOpen())
        throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));
  
  
    bool valid = modem->SendCmd("+CSQ", response);
    if (!valid) {
  
        throw(FailureException("Hardware Error", "Failed to retrieve modem signal strength/quality indicators.", "service_unavailable"));
    }
    modem->SendCmd("+CESQ", response); // return signal strength power
  
    errors = 0;
  
    auto colon = response.find(':');
    auto comma = response.find(',');
  
    if ((colon == std::string::npos) ||
        (comma == std::string::npos)) {
  
        return 0;
    }
  
    result = std::stoi(response.substr(colon + 2, comma - colon - 2));
  
    return result;
}
  
bool SARA_R410M::getInfo(modem_info_t &info)
{
    //bool result = true;
    std::string response;
  
  
    if (!modem->SendCmd("+CGSN", response)) return false; //RETURNS IMEI
    if (response.length() > 2) {
        info.imei = response.substr(0, response.length() - 2);
    }
    else return false;
  
    //signal
    if (!modem->SendCmd("+CSQ", response)) return false;
    auto colon = response.find(':');
    auto comma = response.find(',');
    if ((colon == std::string::npos) || (comma == std::string::npos)) return false;
    info.rssi = std::stoi(response.substr(colon + 2, comma - colon - 2));
  
    //phone
    if (!modem->SendCmd("+CCID", response)) return false;  //THIS RETURNS ICCID
    auto start = response.find(',');
    if (start == std::string::npos) return false;
    auto end = response.find(',', start + 1);
    if (end == std::string::npos) return false;
    start++;
    info.phone = response.substr(start, end - start);
  
    //manufacturer
    if (!modem->SendCmd("+CGMI", response)) return false;//RETURNS MANUFACTURE
    info.manufacturer = response.erase(response.find_last_not_of(" \n\r\t") + 1);
  
    //Model
    if (!modem->SendCmd("+CGMM", response)) return false;//RETURNS MODEL NUMBER
    info.model = response.erase(response.find_last_not_of(" \n\r\t") + 1);
  
    //Version
    if (!modem->SendCmd("+CGMR", response)) return false;//RETURNS SOFTWARE VERSION
    info.softwareVersion = response.erase(response.find_last_not_of(" \n\r\t") + 1);
  
    return true;
}
  
bool SARA_R410M::PowerOn()
{
    digitalWrite(PIN_MODEM_ON, HIGH);
    return true;
}
  
bool SARA_R410M::PowerOff()
{
    digitalWrite(PIN_MODEM_ON, LOW);
    return true;
}
  
bool SARA_R410M::hardReset()
{
#if defined DEBUG_MODEM
    std::cout << "Powering modem OFF" << std::endl;
#endif
    PowerOff();
    std::this_thread::sleep_for(std::chrono::seconds(10));
#if defined DEBUG_MODEM
    std::cout << "Powering modem ON" << std::endl;
#endif
    PowerOn();
#if defined DEBUG_MODEM
    std::cout << "waitForReady()" << std::endl;
#endif
    return waitForReady();
}
  
bool  SARA_R410M::Reset() // This reset routine is not compliant with 4.3.2.0 firmware - Anthony
{
    if (modem == nullptr) {
        throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
    }
    std::string response;
  
    if (!modem->IsOpen())
        throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));
  
    return modem->SendCmd("^RESET", response);
}


void SARA_R410M::enableEcho(){
	std::string res;
	modem->SendCmd("ATE1", res);
	this->enable_echo = true;
}
void SARA_R410M::dissableEcho(){
	std::string res;
	modem->SendCmd("ATE0", res);
	this->enable_echo = false;
}
  

bool SARA_R410M::connect()
{
	std::cout << "HERE?" << std::endl;
    std::string response;
	uint8_t failCount = 0;
	bool success = false;
	while (!success && failCount++ < 7) {
		std::cout << "Waiting for ready" << std::endl;
		success = waitForReady(20);
		if(!success) continue;
		delay_ms(10); // Immense number of arbitrary wait times

		//modem->SendCmd("AT+CGDCONT=1,\"IPV4V6\",\"VZWINTERNET\"", response);
		modem->SendCmd("+CGDCONT=1,\"IPV4V6\",\"\"", response);
		modem->SendCmd("+CGDCONT=2,\"IPV4V6\",\"VZWADMIN\"", response);
		modem->SendCmd("+CGDCONT=3,\"IPV4V6\",\"VZWINTERNET\"", response);
		modem->SendCmd("+CGDCONT=4,\"IPV4V6\",\"VZWAPP\"", response);

		//delay_ms(5000);


		delay_ms(10);
		int i =0;
		for(;i < 20; i++){
			if(modem->SendCmd("+CSQ", response)){
				/*char* num = strtok(line_buffer.lines[this->enable_echo ? 1 : 0], " ");
				num = strtok(NULL, "'");
				int csq = atoi(num);
				if(csq < 99){
					break;
				}*/

				std::cout << "response:\n" << response << ".\n";

				break;
			}

			delay_ms(1000);

		}
		modem->SendCmd("+CESQ", response);
		delay_ms(10);
		modem->SendCmd("+CGPADDR=1", response);
		delay_ms(10);

		success = true;
		success &= modem->SendCmd("+USOCR=6,0 ", response);//socket creation
		delay_ms(10);
		success &= modem->SendCmd("+USOSEC=0" , response);// sls/tls mode configuration
		delay_ms(10);
		success &= modem->SendCmd("+USOSO=0,65535,8,1", response);//set socket option
		delay_ms(10);

		modem->SendCmd("+CGDCONT?", response);
		success = modem->SendCmd("+USOCO=0 ,\"" SERVER_HOST "\", " STR(SERVER_PORT),response, boost::posix_time::seconds(10));
		if(success )
		{
			success &= modem->SendCmd("+USODL=0" , response);
		}
	}
	return success;
}
  
bool SARA_R410M::sendRequest(const std::string &headers, modem_reply_t &message)
{
	std::string response;
	bool success = connect();
   
	if (success) {
			
		success = modem->SendData(headers + message.request_messageBody );

		if (!success)
		{
			modem->exitDataMode();
			//modem->SendCmd("+USOCL=0",response);//CLOSE SOCKET COMMAND

			return false;


		}

		std::string data;
		std::cout << "poop 2" << std::endl;
		if (!modem->RecvData(data, boost::posix_time::seconds(10))) {
			modem->exitDataMode();
			//modem->SendCmd("+USOCL=0", response);
			return false;

		} else {
			std::cout << "Poop 3" << std::endl;
			modem->exitDataMode();
			//modem->SendCmd("+USOCL=0", response);

		}
		bool valid_http = data.find("HTTP/1.1 ") == 0;

		if (valid_http && (data.length() >= 20)) {
			std::cout << "REPLY LENGTH: " << data.length() << std::endl;
			message.statusCode = std::stoi(data.substr(9, 3));
			message.reasonPhrase = data.substr(13, data.find("\r\n") - 13);
			message.response_headers = data.substr(data.find("\r\n") + 2, data.find("\r\n\r\n") - data.find("\r\n") - 2);
			int length_pos = data.find("Content-Length:") + 16;
			int length = std::stoi(data.substr(length_pos,data.find("\r\n",length_pos)-length_pos));
			size_t index = data.find("\r\n\r\n");
			data = data.substr(0, index+4+length);
			if (index > 0) {
				index += 4;
				if (data.find("Transfer-Encoding: chunked") != std::string::npos) {
					std::cout << "Receiving Chunked data" << std::endl;
					std::string delimiter = "\r\n";
					size_t pos = index, prev_pos = index;
					std::string token;
					while ((pos = data.find(delimiter, prev_pos + 1)) != std::string::npos) {
						token = data.substr(prev_pos, pos - prev_pos);
						int len = std::stoul(token, nullptr, 16);
						pos += delimiter.length();
						message.response_messageBody += data.substr(pos, len);
						pos += len;
						pos += delimiter.length();
						prev_pos = pos;
					}
				}
				else {
					message.response_messageBody = data.substr(index);
				}
			}
		}
		else {
			message.statusCode = 502;
			message.reasonPhrase = "Bad Gateway";
			message.response_headers = "";
			message.response_messageBody = data;
		}

		delay_ms(10);
			
#if defined DEBUG_MODEM
		std::cout << "HeartBeat success!" << std::endl;
#endif
           
        modem->SendCmd("+USOCL=0", response);
        return true;
    }
	
		
	modem->SendCmd("+USOCL=0", response);
	
	return false;
}
