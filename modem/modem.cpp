#include  "modem.h"
#include "failure_exception.h"

#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <wiringPi.h>
#include <thread>
#include "modem_driver.h"

using boost::lexical_cast;


using namespace boost;

//#if defined DEBUG_MODEM
#define TRACE_EN
#define TRACE_INFO(...)    do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
#define TRACE_DEBUG(...)   do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
//#else
//#define TRACE_INFO(...)
//#define TRACE_DEBUG(...)
//#endif

static uint8_t  errors = 0;

// re-invent the time function (better than solar roadways though...)
static inline posix_time::ptime ptime (void)
{
	return posix_time::microsec_clock::local_time();
}

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

Modem::Modem(){
	this->modem = new ModemDriver();
	fail1 = fail2 = fail3 = fail4 = fail5 = 0;
}
Modem::~Modem(){
	delete this->modem;
	this->modem = nullptr;
}

void Modem::init()
{
	pinMode(PIN_MODEM_DTR, OUTPUT);										/* P1_37: SKY_DTR										*/
	pinMode(PIN_MODEM_RTS, OUTPUT);										/* P1_36: SKY_RTS										*/
	pinMode(PIN_MODEM_ON, OUTPUT);										/* P1_38: SKY_ON										*/

	digitalWrite(PIN_MODEM_RTS, LOW);
	digitalWrite(PIN_MODEM_ON, HIGH);										
}
bool Modem::Open(const std::string &device, int baudrate)
{
	return modem != nullptr && modem->Open(device, baudrate); 
}

bool Modem::waitForReady(time_t timeout)
{
	time_t start_time = time(nullptr);
	std::string response;
	int state = 0;
	
	if(modem->WaitURC("^SYSSTART",response, boost::posix_time::seconds(timeout))){
		return true;
	}

	/*if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(120))){
		return true;
	}*/ //firmware version 4.3.3.0 no longer has SQNREGISTEREDIMPU URC... much faster - anthony
	std::this_thread::sleep_for(std::chrono::seconds(1));
	
	return false;
}

bool Modem::ohShit(int fail_state)
{
	std::string response;
	while(fail_state != 7){
		
		//escalate fail_state depending on current fail counters
#if defined DEBUG_MODEM
		std::cout << "INFO\tCurrently in state " << fail_state << std::endl;
		std::cout << "INFO\tFail1: " << fail1 << ", Fail2: " << fail2 << ", Fail3: " << fail3 << ", Fail4: " << fail4 << ", Fail5: " << fail5 << std::endl;
#endif
		switch(fail_state){
			case 8:{ //changed case number so i never runs. originally 1. - Anthony
				modem->SendCmd("+CEER", response);
#if defined DEBUG_MODEM
				std::cout << "CEER: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CSQ", response);
#if defined DEBUG_MODEM
				std::cout << "CSQ: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CESQ", response);
#if defined DEBUG_MODEM
				std::cout << "CESQ: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CGPADDR=1", response);
#if defined DEBUG_MODEM
				std::cout << "CGPADDR: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+COPS?", response, boost::posix_time::seconds(15)); // Arbitrary wait time
#if defined DEBUG_MODEM
				std::cout << "COPS: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("^SICA=0,3", response);
#if defined DEBUG_MODEM
				std::cout << "SICA=0,3: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				
				if(fail1 >= 1){
					fail_state = 3;
#if defined DEBUG_MODEM
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
					break;
				}
				
				modem->SendCmd("+CGATT=0", response);
#if defined DEBUG_MODEM
				std::cout << "CGATT=0: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail1++;
				modem->SendCmd("+CGATT=1", response);
#if defined DEBUG_MODEM
				std::cout << "CGATT=1: " << response << std::endl;
#endif
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					return true;
				}
				
				fail_state = 3;
#if defined DEBUG_MODEM
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
				break;
				
				
				/*
				modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if(!modem->SendCmd("^SICA=0,3", response)){
					fail_state = 3;
					break;
				}
				// Additional troubleshooting commands & responses
				if(!modem->SendCmd("+CGATT=0", response)){
					fail_state = 3;
					break;
				}
				std::this_thread::sleep_for(std::chrono::seconds(10)); // Arbitrary wait time
				if(!modem->SendCmd("+CGATT=1", response)){
					fail_state = 3;
					break;
				}
				if(!modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(120))){
					fail_state = 3;
					break;
				}
				
				std::this_thread::sleep_for(std::chrono::seconds(1));

				fail1++;
				return true;
				break;*/
			}
			case 9:{ //changed case number so it never runs. Originally 2 - Anthony
				modem->SendCmd("+CEER", response);
#if defined DEBUG_MODEM
				std::cout << "CEER: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CSQ", response);
#if defined DEBUG_MODEM
				std::cout << "CSQ: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CESQ", response);
#if defined DEBUG_MODEM
				std::cout << "CESQ: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CGPADDR=1", response);
#if defined DEBUG_MODEM
				std::cout << "CGPADDR: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+COPS?", response, boost::posix_time::seconds(15)); // Arbitrary wait time
#if defined DEBUG_MODEM
				std::cout << "COPS: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+SISC=0", response);
#if defined DEBUG_MODEM
				std::cout << "SISC=0: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("^SICA=0,3", response);
#if defined DEBUG_MODEM
				std::cout << "SICA=0,3: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				
				if(fail2 >= 1){
					fail_state = 3;
#if defined DEBUG_MODEM
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
					break;
				}
				
				modem->SendCmd("+CGATT=0", response);
#if defined DEBUG_MODEM
				std::cout << "CGATT=0: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail2++;
				modem->SendCmd("+CGATT=1", response);
#if defined DEBUG_MODEM
				std::cout << "CGATT=1: " << response << std::endl;
#endif
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					return true;
				}
				
				fail_state = 3;
#if defined DEBUG_MODEM
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
				break;
				
				/*modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SICA=0,3", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CFUN=0", response);//Switch to 'Minimum Functionality'  mode
				std::this_thread::sleep_for(std::chrono::seconds(10)); // Arbitrary wait time
				modem->SendCmd("+CFUN=1", response);//Switch to Normal mode
				if(!modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(120))){
					fail_state = 3;
					break;
				}
				
				fail2++;
				fail1 = 0;
				return true;				
				break;*/
			}
			case 10:{ //changed case number so it never runs. originally 3. - anthony
				if(fail3 >= 1){
					fail_state = 4;
#if defined DEBUG_MODEM
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
					break;
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				modem->SendCmd("+CFUN=0", response);
#if defined DEBUG_MODEM
				std::cout << "CFUN=0: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail3++;
				modem->SendCmd("+CFUN=1", response);
#if defined DEBUG_MODEM
				std::cout << "CFUN=1: " << response << std::endl;
#endif
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					fail1 = fail2 = 0;
					return true;
				}
				
				fail_state = 4;
#if defined DEBUG_MODEM
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
				break;
				
				
				
				/*modem->SendCmd("+CEER", response);
				std::cout << "CEER: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SICA=0,3", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CFUN=1,1", response);
				
				if(!waitForReady()){
					fail_state = 4;
					break;
				}

				fail3++;
				fail2 = 0;
				fail1 = 0;
				return true;
				break;*/
			}
			case 1:{ // changed case number from 4 to 1 so it runs first. - anthony
				if(fail1 >= 1){
					fail_state = 2;
#if defined DEBUG_MODEM
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
					break;
				}
				
				fail1++; // was fail4 - anthony
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				// inserting band selection test code here - Anthony
				modem->SendCmd("^SCFG=\"Radio/Band\",\"33619968\",\"1\"", response);
				// 33619968 enables ALL bands on the modem to be utilized - Anthony
				// 1 makes the band selection effective immediately without reboot - Anthony
#if defined DEBUG_MODEM
				std::cout << "Radio/Band: " << response << std::endl;
#endif
				std::this_thread::sleep_for(std::chrono::seconds(2)); // arbitrary wait time - anthony
				// end of band selection test code
				
				modem->SendCmd("+CFUN=1,1", response);
#if defined DEBUG_MODEM
				std::cout << "CFUN=1,1: " << response << std::endl;
#endif
				
				if(waitForReady()){
					fail1 = fail2 = fail3 = 0;
					return true;
				}
				
				fail_state = 2;
#if defined DEBUG_MODEM
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
				break;
				
				
				
				/*fail3 = 0;
				fail2 = 0;
				fail1 = 0;
				return false;*/
			}
			case 2:{ //changed case number from 5 to 2 so it runs after 1 (4) - anthony
				// Power cycle modem
				if(fail2 >= 1){
					fail_state = 1; //go back to case 1 (4) because no other options... - anthony
#if defined DEBUG_MODEM
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
#endif
					break;
				}
				
				fail2++;
				//fail5++; // case 6 not supported. do not uncomment. - anthony
				if(hardReset()) {
					fail1 = fail2 = fail3 = fail4 = 0;
					return true;
				}
				
				fail1 = fail2 = fail3 = fail4 = 0;
				//fail_state = 6;
#if defined DEBUG_MODEM
				//std::cout << "INFO\tswitching to state " << fail_state << std::endl;
				std::cout << "Accepting Defeat..." << fail_state << std::endl;
#endif
				return true; // Accepting defeat
			}
			case 6:{ // Complete failure. Output high SPL 1kHz tone from box to alert nearby techs. Or reboot RPi.
				fail1=0;
				fail2=0;
				fail3=0;
				fail4=0;
				fail5=0;
				return true; // Accepting defeat
			}
			default:
				return false;
		}
	}
}

void  Modem::set_error (void)
{
	if(modem == nullptr) return;
	std::string response;


	errors++;

	if (errors == 3) {
		errors = 0;

		TRACE_DEBUG("Resetting modem on command errors.");

		digitalWrite(PIN_MODEM_ON, HIGH);
		std::this_thread::sleep_for(std::chrono::seconds(6));
		digitalWrite(PIN_MODEM_ON, LOW);

		TRACE_DEBUG("Waiting for modem to boot.");

		std::this_thread::sleep_for(std::chrono::seconds(15));

		modem->WaitURC("^", response, boost::posix_time::seconds(90));

#if defined DEBUG_MODEM
		std::cout << "URC: " << response << std::endl;
#endif
		throw(FailureException("Modem Hardware Error", "Resetting modem on command errors.", "service_unavailable"));
	}
}


modem_device_t Modem::Device()
{
	modem_device_t result;
	std::string  response;
	bool valid;
	
	if(modem == nullptr){
		throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
	}


	if (!modem->IsOpen())
		throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));

	valid = modem->SendCmd("+CGMI", response);
	if (!valid) {
		set_error();

		throw(FailureException("Hardware Error", "Failed to retrieve modem manufacturer name.", "service_unavailable"));
	}

	errors = 0;

	if (response.length() > 2) {
		result.manufacturer = response.substr(0, response.length() - 2);
	} else {
		result.manufacturer = "<error>";
	}

	valid = modem->SendCmd("+CGMM", response);
	if (!valid)
		throw(FailureException("Hardware Error", "Failed to retrieve modem model name.", "service_unavailable"));

	if (response.length() > 2) {
		result.model = response.substr(0, response.length() - 2);
	} else {
		result.model  = "<error>";
	}

	valid = modem->SendCmd("+CGMR", response);
	if (!valid)
		throw(FailureException("Hardware Error", "Failed to retrieve modem software version.", "service_unavailable"));

	if (response.length() > 2) {
		result.softwareVersion = response.substr(0, response.length() - 2);
	} else {
		result.softwareVersion = "<error>";
	}

	return result;
}

std::string Modem::Phone ()
{
	std::string result;
	std::string response;
	if(modem == nullptr){
		throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
	}


	if (!modem->IsOpen())
		throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));


	bool valid = modem->SendCmd("+CNUM", response);
	if (!valid) {
		set_error();

		throw(FailureException("Hardware Error", "Failed to retrieve modem phone number.", "service_unavailable"));
	}

	errors = 0;

	if (response.length() > 0) {
		auto  start = response.find(',');
		if (start == std::string::npos) {
			result = "<invalid>";
		} else {
			std::string cnum = response.substr(start + 1);
			auto end = cnum.find(',');
			if (end == std::string::npos) {
				result = "<invalid>";
			} else {
				result = cnum.substr(0, end);
			}
		}
	} else {
		result = "<error>";
	}
	return result;
}

std::string Modem::IMEI ()
{
	std::string result;
	std::string response;


	if (!modem->IsOpen())
		throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));


	bool valid = modem->SendCmd("+CGSN", response);
	if (!valid) {
		set_error();

		throw(FailureException("Hardware Error", "Failed to retrieve modem imei.", "service_unavailable"));
	}

	errors = 0;

	if (response.length() > 2) {
		result = response.substr(0, response.length() - 2);
	} else {
		result = "<error>";
	}

	return result;
}

int Modem::Signal ()
{
	if(modem == nullptr){
		throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
	}
	
	int result = 0;
	std::string response;

	if (!modem->IsOpen())
		throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));


	bool valid = modem->SendCmd("+CSQ", response);
	if (!valid) {
		set_error();

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

bool Modem::getInfo(modem_info_t &info)
{
	//bool result = true;
	std::string response;
	
	//IMEI  Just returns ERROR
	if(!modem->SendCmd("+CGSN", response)) return false;
	if (response.length() > 2) {
		info.imei = response.substr(0, response.length() - 2);
	} else return false; 
	
	//signal
	if(!modem->SendCmd("+CSQ", response)) return false;
	auto colon = response.find(':');
	auto comma = response.find(',');
	if((colon == std::string::npos)||(comma == std::string::npos)) return false;
	info.rssi = std::stoi(response.substr(colon + 2, comma - colon - 2));
	
	//phone
	if(!modem->SendCmd("+CNUM", response)) return false;
	auto start = response.find(',');
	if(start == std::string::npos) return false;
	auto end = response.find(',', start+1);
	if(end == std::string::npos) return false;
	start++;
	info.phone = response.substr(start, end-start);
	
	//manufacturer
	if(!modem->SendCmd("+CGMI", response)) return false;
	info.manufacturer = response.erase(response.find_last_not_of(" \n\r\t")+1);
	
	//Model
	if(!modem->SendCmd("+CGMM", response)) return false;
	info.model = response.erase(response.find_last_not_of(" \n\r\t")+1);
	
	//Version
	if(!modem->SendCmd("+CGMR", response)) return false;
	info.softwareVersion = response.erase(response.find_last_not_of(" \n\r\t")+1);
	
	return true;
}

bool Modem::PowerOn ()
{
	digitalWrite(PIN_MODEM_ON, HIGH);
	return true;
}

bool Modem::PowerOff ()
{
	digitalWrite(PIN_MODEM_ON, LOW);	
	return true;
}

bool Modem::hardReset()
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

bool  Modem::Reset () // This reset routine is not compliant with 4.3.2.0 firmware - Anthony
{
	if(modem == nullptr){
		throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
	}
	std::string response;

	if (!modem->IsOpen())
		throw(FailureException("Modem Hardware Error", "Serial device is not open.", "service_unavailable"));

	return modem->SendCmd("^RESET", response);
}


bool Modem::sendRequest(const std::string &headers, modem_reply_t &message)
{
	std::string response;
	fail1 = fail2 = fail3 = fail4 = fail5 = 0;
	while(true){
		std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Immense number of arbitrary wait times
		modem->SendCmd("+CGDCONT=3,\"IPV4V6\",\"NIMBLINK.GW12.VZWENTP\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		modem->SendCmd("+CGDCONT?", response);
		if(response.find("NIMBLINK.GW12.VZWENTP") == std::string::npos);//error

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		modem->SendCmd("^SICA=1,3", response);
		std::this_thread::sleep_for(std::chrono::seconds(5)); // Arbitrary wait time
		modem->SendCmd("^SICA?", response);
		if(response.find("3,1") == std::string::npos){
#if defined DEBUG_MODEM
			std::cout << "ERROR\t" << "AT^SICA? failed. executing ohShit(1)" << std::endl;
#endif
			if(!ohShit(1)) return false;
			else continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		
		modem->SendCmd("+CSQ", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		modem->SendCmd("+CESQ", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		modem->SendCmd("+CGPADDR=1", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		
		bool success = true;

		success &= modem->SendCmd("^SISS=0,\"srvType\",\"Socket\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		success &= modem->SendCmd("^SISS=0,\"conId\",3", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		success &= modem->SendCmd("^SISS=0,\"address\",\"socktcp://" + message.host + ":" + std::to_string(message.port) + ";etx\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		
		if(!success) {
			if(!ohShit(1)) return false;
			else continue;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		success = modem->SendCmd("^SISO=0", response);
		if(!success) {
			if(!ohShit(1)) return false;
			else continue;
		}
		
#if defined DEBUG_MODEM
		std::cout << "Response: " << response << std::endl;
#endif

		success = modem->WaitURC("^SISW: 0,1", response, boost::posix_time::seconds(20));
		if(!success){
#if defined DEBUG_MODEM
			std::cout << "ERROR\t" << "Socket setup failed. executing ohShit(1)" << std::endl;
#endif
			if(!ohShit(1)) return false;
			else continue;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		if(!modem->SendCmd("^SIST=0", response)){
#if defined DEBUG_MODEM
			std::cout << "ERROR\t" << "SIST failed. executing ohShit(1)" << std::endl;
#endif
			if(!ohShit(1)) return false;
			else continue;
		}
		success = 1;
		int send_length = headers.length() + message.request_messageBody.length();
		if(headers.size() > 0)success = modem->SendData(headers);
		success &= modem->SendData(message.request_messageBody);
		std::cout << "SEND LENGTH: " << send_length << std::endl;
		//success &= modem->SendData("\r\n");
		if(!success){
			modem->exitDataMode();
			//std::this_thread::sleep_for(std::chrono::milliseconds(1100));
			modem->SendCmd("^SISC=0", response);
#if defined DEBUG_MODEM
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
#endif
			if(!ohShit(1)) return false;
			else continue;
		}
		
		std::string data;
		if(!modem->RecvData(data, boost::posix_time::seconds(90))){
			modem->exitDataMode();
			//std::this_thread::sleep_for(std::chrono::milliseconds(1100));
			modem->SendCmd("^SISC=0", response);
#if defined DEBUG_MODEM
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
#endif
			if(!ohShit(1)) return false;
			else continue;
		} else {
			modem->exitDataMode();
			//std::this_thread::sleep_for(std::chrono::milliseconds(1100));
		}
		modem->SendCmd("^SISC",response);
		std::cout << "HERE HERE HERE" << std::endl;
		bool valid_http = data.find("HTTP/1.1 ") == 0;
		
		if (valid_http && (data.length() >= 20)) {
			std::cout << "REPLY LENGTH: " << data.length() << std::endl;
			message.statusCode = std::stoi(data.substr(9, 3));
			message.reasonPhrase = data.substr(13, data.find("\r\n") - 13);
			message.response_headers = data.substr(data.find("\r\n") + 2, data.find("\r\n\r\n") - data.find("\r\n") - 2);
			size_t index = data.find("\r\n\r\n");
			if(index > 0){
				index += 4;
				if(data.find("Transfer-Encoding: chunked") != std::string::npos){
					std::cout << "Receiving Chunked data" << std::endl;
					std::string delimiter = "\r\n";
					size_t pos = index, prev_pos = index;
					std::string token;
					while ((pos = data.find(delimiter, prev_pos+1)) != std::string::npos) {
						token = data.substr(prev_pos, pos-prev_pos);
						int len = std::stoul(token, nullptr, 16);
						pos += delimiter.length();
						message.response_messageBody += data.substr(pos, len);
						pos += len;
						pos += delimiter.length();
						prev_pos = pos;
					}
				} else {
					message.response_messageBody = data.substr(index);
				}
			}
		} else {
			message.statusCode = 502;
			message.reasonPhrase = "Bad Gateway";
			message.response_headers = "";
			message.response_messageBody = data;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		modem->SendCmd("^SISC=0", response);
#if defined DEBUG_MODEM
		std::cout << "HeartBeat success!" << std::endl;
#endif
		//modem->SendCmd("^SICA=0,3", response); // Keeping the PDP context active in this test - Anthony

		return true;
	}
}