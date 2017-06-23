#include  "modem.h"
#include "failure_exception.h"

#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <wiringPi.h>
#include <thread>
#include "modem_driver.h"

using boost::lexical_cast;


using namespace boost;


#define TRACE_EN
#define TRACE_INFO(...)    do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)
#define TRACE_DEBUG(...)   do { auto _ts = ptime().time_of_day().total_milliseconds(); TRACE("[", _ts, "] ", __VA_ARGS__); } while (0)


static uint8_t  errors = 0;

// re-invent the time function (better than solar roadways though...)
static inline posix_time::ptime ptime (void)
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
	
	modem->WaitURC("^SYSSTART",response, boost::posix_time::seconds(timeout));
	// inserting band selection test code here - Anthony
	modem->SendCmd("^SCFG=\"Radio/Band\",\"33554432\",\"1\"", response);
	std::cout << "Radio/Band: " << response << std::endl;
	// end of band selection test code
	if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
		return true;
	}
	std::this_thread::sleep_for(std::chrono::seconds(1));
	
	return false;
}

bool Modem::ohShit(int fail_state)
{
	std::string response;
	while(fail_state != 7){
		
		//escalate fail_state depending on current fail counters
		std::cout << "INFO\tCurrently in state " << fail_state << std::endl;
		std::cout << "INFO\tFail1: " << fail1 << ", Fail2: " << fail2 << ", Fail3: " << fail3 << ", Fail4: " << fail4 << ", Fail5: " << fail5 << std::endl;
		
		switch(fail_state){
			case 1:{
				modem->SendCmd("+CEER", response);
				std::cout << "CEER: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CSQ", response);
				std::cout << "CSQ: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CESQ", response);
				std::cout << "CESQ: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CGPADDR=1", response);
				std::cout << "CGPADDR: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+COPS?", response, boost::posix_time::seconds(15)); // Arbitrary wait time
				std::cout << "COPS: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SICA=0,3", response);
				std::cout << "SICA=0,3: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				
				if(fail1 >= 1){
					fail_state = 3;
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
					break;
				}
				
				modem->SendCmd("+CGATT=0", response);
				std::cout << "CGATT=0: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail1++;
				modem->SendCmd("+CGATT=1", response);
				std::cout << "CGATT=1: " << response << std::endl;
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					return true;
				}
				
				fail_state = 3;
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
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
			case 2:{
				modem->SendCmd("+CEER", response);
				std::cout << "CEER: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CSQ", response);
				std::cout << "CSQ: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CESQ", response);
				std::cout << "CESQ: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CGPADDR=1", response);
				std::cout << "CGPADDR: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+COPS?", response, boost::posix_time::seconds(15)); // Arbitrary wait time
				std::cout << "COPS: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+SISC=0", response);
				std::cout << "SISC=0: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SICA=0,3", response);
				std::cout << "SICA=0,3: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				
				if(fail2 >= 1){
					fail_state = 3;
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
					break;
				}
				
				modem->SendCmd("+CGATT=0", response);
				std::cout << "CGATT=0: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail2++;
				modem->SendCmd("+CGATT=1", response);
				std::cout << "CGATT=1: " << response << std::endl;
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					return true;
				}
				
				fail_state = 3;
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
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
			case 3:{
				if(fail3 >= 1){
					fail_state = 4;
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
					break;
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CFUN=0", response);
				std::cout << "CFUN=0: " << response << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
				fail3++;
				modem->SendCmd("+CFUN=1", response);
				std::cout << "CFUN=1: " << response << std::endl;
				
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(90))){
					fail1 = fail2 = 0;
					return true;
				}
				
				fail_state = 4;
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
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
			case 4:{
				if(fail4 >= 1){
					fail_state = 5;
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
					break;
				}
				
				fail4++;
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CFUN=1,1", response); // Does this command return a response? - Anthony
				std::cout << "CFUN=1,1: " << response << std::endl;
				
				if(waitForReady()){
					fail1 = fail2 = fail3 = 0;
					return true;
				}
				
				fail_state = 5;
				std::cout << "INFO\tswitching to state " << fail_state << std::endl;
				break;
				
				
				
				/*fail3 = 0;
				fail2 = 0;
				fail1 = 0;
				return false;*/
			}
			case 5:{
				// Power cycle modem
				if(fail5 >= 1){
					fail_state = 6;
					std::cout << "INFO\tswitching to state " << fail_state << std::endl;
					break;
				}
				
				//fail5++; // Accepting defeat
				if(hardReset()) {
					fail1 = fail2 = fail3 = fail4 = 0;
					return true;
				}
				
				fail1 = fail2 = fail3 = fail4 = 0;
				//fail_state = 6;
				//std::cout << "INFO\tswitching to state " << fail_state << std::endl;
				std::cout << "Accepting Defeat..." << fail_state << std::endl;
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

		std::cout << "URC: " << response << std::endl;

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
	std::cout << "Powering modem OFF" << std::endl;
	PowerOff();
	std::this_thread::sleep_for(std::chrono::seconds(10));
	std::cout << "Powering modem ON" << std::endl;
	PowerOn();
	std::cout << "waitForReady()" << std::endl;
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
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Immense number of arbitrary wait times
		modem->SendCmd("+CGDCONT=3,\"IPV4V6\",\"NIMBLINK.GW12.VZWENTP\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		modem->SendCmd("+CGDCONT?", response);
		if(response.find("NIMBLINK.GW12.VZWENTP") == std::string::npos);//error

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		modem->SendCmd("^SICA=1,3", response);
		std::this_thread::sleep_for(std::chrono::seconds(5)); // Arbitrary wait time
		modem->SendCmd("^SICA?", response);
		if(response.find("3,1") == std::string::npos){
			std::cout << "ERROR\t" << "AT^SICA? failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
		modem->SendCmd("+CSQ", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		modem->SendCmd("+CESQ", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		modem->SendCmd("+CGPADDR=1", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
		bool success = true;

		success &= modem->SendCmd("^SISS=0,\"srvType\",\"Socket\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		success &= modem->SendCmd("^SISS=0,\"conId\",3", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		success &= modem->SendCmd("^SISS=0,\"address\",\"socktcp://" + message.host + ":" + std::to_string(message.port) + ";etx\"", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
		if(!success) {
			if(!ohShit(2)) return false;
			else continue;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		success = modem->SendCmd("^SISO=0", response);
		if(!success) {
			if(!ohShit(2)) return false;
			else continue;
		}
		
		std::cout << "Response: " << response << std::endl;

		success = modem->WaitURC("^SISW: 0,1", response, boost::posix_time::seconds(20));
		if(!success){
			std::cout << "ERROR\t" << "Socket setup failed. executing ohShit(1)" << std::endl;
			if(!ohShit(2)) return false;
			else continue;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if(!modem->SendCmd("^SIST=0", response)){
			std::cout << "ERROR\t" << "SIST failed. executing ohShit(1)" << std::endl;
			if(!ohShit(2)) return false;
			else continue;
		}
		
		success = modem->SendData(headers);
		success &= modem->SendData(message.request_messageBody);
		if(!success){
			modem->exitDataMode();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("^SISC=0", response);
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
			if(!ohShit(2)) return false;
			else continue;
		}
		
		std::string data;
		if(!modem->RecvData(data, boost::posix_time::seconds(90))){
			modem->exitDataMode();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("^SISC=0", response);
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
			if(!ohShit(2)) return false;
			else continue;
		}

		bool valid_http = data.find("HTTP/1.1 ") == 0;
		
		if (valid_http && (data.length() >= 20)) {
			message.statusCode = std::stoi(data.substr(9, 3));
			message.reasonPhrase = data.substr(13, data.find("\r\n") - 13);
			message.response_headers = data.substr(data.find("\r\n") + 2, data.find("\r\n\r\n") - data.find("\r\n") - 2);
			size_t index = data.find("\r\n\r\n");
			if(index > 0){
				message.response_messageBody = data.substr(index + 4);
			}
		} else {
			message.statusCode = 502;
			message.reasonPhrase = "Bad Gateway";
			message.response_headers = "";
			message.response_messageBody = data;
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		modem->SendCmd("^SISC=0", response);
		std::cout << "HeartBeat success!" << std::endl;
		//modem->SendCmd("^SICA=0,3", response); // Keeping the PDP context active in this test - Anthony

		return true;
	}
}