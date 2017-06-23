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

// re-invent the time function 
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
	fail0 = fail1 = fail2 = fail3 = fial0 = 0;
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
//	std::this_thread::sleep_for(std::chrono::seconds(6));
	//digitalWrite(PIN_MODEM_ON, LOW);										/* Power on modem.										*/
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
	do{
		//switch(state)
		//{
		//	case 0:
		modem->WaitURC("^SYSSTART",response, boost::posix_time::seconds(timeout));
		modem->SendCmd("+CSQ", response);
				if(modem->WaitURC("SQNREGISTEREDIMPU",response, boost::posix_time::seconds(timeout))){
					//std::this_thread::sleep_for(std::chrono::seconds(5));
					return true;
				}
		//	case 1:
		//		if(modem->SendCmd("+CGDCONT?", response)){
		//			return true;
		//		}
		//}
		std::this_thread::sleep_for(std::chrono::seconds(1));
		//std::cout << "UNDATA: " << response << std::endl;
		//if(modem->SendCmd("", response)) return true;
	}while(time(nullptr) - start_time <= timeout);
	return false;
}

bool Modem::ohShit(int fail_state)
{
	std::string response;
	while(fail_state != 4){
		
		//escalate fail_state depending on current fail counters
		std::cout << "INFO\tCurrently in state " << fail_state << std::endl;
		std::cout << "INFO\tFial0: " << fial0 << ", Fail0: " << fail0 << ", Fail1: " << fail1 << ", Fail2: " << fail2 << ", Fail3: " << fail3 << std::endl;
		
		if(fial0 >= 4){
			fail_state = 4;
			std::cout << "INFO\tswitching to state " << fail_state << std::endl;
		}
		if(fail0 >= 10){
			fail_state = 2;
			std::cout << "INFO\tswitching to state " << fail_state << std::endl;
		}
		if(fail1 >= 3){
			fail_state = 2;
			std::cout << "INFO\tswitching to state " << fail_state << std::endl;
		}
		if(fail2 >= 2){
			fail_state = 3;
			std::cout << "INFO\tswitching to state " << fail_state << std::endl;
		}
		if(fail3 >= 1){
			fail_state = 4;
			std::cout << "INFO\tswitching to state " << fail_state << std::endl;
		}
		
		switch(fail_state){
			case 0:{
				modem->SendCmd("+CEER", response);
				std::cout << "ERROR STATE 0: " << response << std::endl;
				
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if(!modem->SendCmd("+CSQ", response)){
					fial0++;
					break;
				}
				auto colon = response.find(':');
				auto comma = response.find(',');
				if((colon == std::string::npos)||(comma == std::string::npos)){
					fial0++;
					break;
				}
				if(std::stoi(response.substr(colon + 2, comma - colon - 2)) == 99){
					fial0++;
					break;
				}
				
				if(!modem->SendCmd("+COPS=?", response, boost::posix_time::seconds(20))){
					std::this_thread::sleep_for(std::chrono::seconds(10));
					fial0++;
					break;
				}
				
				std::this_thread::sleep_for(std::chrono::seconds(1));
				
				
				fail0++;
				fial0 = 0;
				return true;
				break;
			}
			case 1:{
				modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				if(!modem->SendCmd("^SICA=0,3", response)){
					fail_state = 3;
					break;
				}
				
				std::this_thread::sleep_for(std::chrono::seconds(1));

				fail1++;
				fail0 = 0;
				fial0 = 0;
				return true;
				break;
			}
			case 2:{
				modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("^SICA=0,3", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				modem->SendCmd("+CFUN=4,0", response);//Switch to Airplane mode
				std::this_thread::sleep_for(std::chrono::seconds(5));
				modem->SendCmd("+CFUN=1,0", response);//Switch to Normal mode
				std::this_thread::sleep_for(std::chrono::seconds(5));
				
				
				
				fail2++;
				fail1 = 0;
				fail0 = 0;
				fial0 = 0;
				return true;				
				break;
			}
			case 3:{
				modem->SendCmd("+CEER", response);
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
				fail0 = 0;
				fial0 = 0;
				return true;
				break;
			}
			case 4:
				fail3 = 0;
				fail2 = 0;
				fail1 = 0;
				fail0 = 0;
				fial0 = 0;
				return false;
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

		modem->WaitURC("^", response, boost::posix_time::seconds(30));

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

	errors = 0;

	/*if (response.length() < 3) {
		result.rssi = "<error>";
		result.ber = "<error>";

		return result;
	}*/

	auto colon = response.find(':');
	auto comma = response.find(',');

	if ((colon == std::string::npos) ||
		(comma == std::string::npos)) {

		return 0;
	}

	result = std::stoi(response.substr(colon + 2, comma - colon - 2));
//	result.ber = std::stoi(response.substr(comma + 1, response.length() - comma - 3));

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
	digitalWrite(PIN_MODEM_ON, LOW);
	return true;
}

bool Modem::PowerOff ()
{
	digitalWrite(PIN_MODEM_ON, HIGH);	
	return true;
}

void Modem::hardReset()
{
	PowerOff();
	std::this_thread::sleep_for(std::chrono::seconds(10));
	PowerOn();
	waitForReady();
	//std::this_thread::sleep_for(std::chrono::seconds(2));
}

bool  Modem::Reset ()
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
	fail0 = fail1 = fail2 = fail3 = fial0 = 0;
	while(true){
		modem->SendCmd("+CGDCONT=3,\"IPV4V6\",\"NIMBLINK.GW12.VZWENTP\"", response);
		std::this_thread::sleep_for(std::chrono::seconds(1));
		modem->SendCmd("+CGDCONT?", response);
		if(response.find("NIMBLINK.GW12.VZWENTP") == std::string::npos);//error
		int signal = Signal();
		if(!signal || signal == 99);//error
		std::cout << "INFO\tSignal: " << signal << std::endl;
		
		/*std::this_thread::sleep_for(std::chrono::seconds(2));
		if(!modem->SendCmd("+CGPADDR=3", response)){
			std::cout << "ERROR\t" << "AT+CGPADDR=3 failed. executing ohShit(0)" << std::endl;
			if(!ohShit(0)) return false;
			else continue;
		}*/
		std::cout << "INFO\tIP ADDRESS: " << response << std::endl;
		modem->SendCmd("^SICA=1,3", response);
		std::this_thread::sleep_for(std::chrono::seconds(5));
		modem->SendCmd("^SICA?", response);
		if(response.find("3,1") == std::string::npos){
			std::cout << "ERROR\t" << "AT^SICA? failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}

		/*if(!modem->SendCmd("^SISI?", response)){
			std::cout << "ERROR\t" << "AT^SISI? failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
			
		if(response.length() && response.find("^SISI: 0,2") != std::string::npos){
			modem->SendCmd("^SISC=0", response);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}*/
		
		bool success = true;
		//for(int retry = 0; retry < 3; retry++){
		//	if(retry){
		//		modem->SendCmd("^SISC=0", response);
		//		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//	}
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		success &= modem->SendCmd("^SISS=0,\"srvType\",\"Socket\"", response);
		
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		success &= modem->SendCmd("^SISS=0,\"conId\",3", response);
		
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		success &= modem->SendCmd("^SISS=0,\"address\",\"socktcp://" + message.host + ":" + std::to_string(message.port) + ";etx\"", response);
		
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		if(!success) {
			if(!ohShit(1)) return false;
			else continue;
		}

		//std::this_thread::sleep_for(std::chrono::milliseconds(500));
		success = modem->SendCmd("^SISO=0", response);
		if(!success) {
			if(!ohShit(1)) return false;
			else continue;
		}
		
		std::cout << "Response: " << response << std::endl;
		
		//	for(int retry1 = 0; retry1 < 5; retry1++){
		//		
		//		success = modem->SendCmd("^SISW=?", response);
		//		if(success) break;
		//	}
			
		//	if(!success) continue;
			
		/*	success = modem->WaitURC("^SISW:", response, boost::posix_time::seconds(5));  // this never happens
			modem->SendCmd("^SISO?", response);
			if(success){
				if(response.find("^SISW: 0,1") == std::string::npos){
					success = false;
					continue;
				}
			}*/
			
			//std::this_thread::sleep_for(std::chrono::seconds(5));
			
			
			
			//modem->SendCmd("^SISO?", response);
		//	break;
		//}
		success = modem->WaitURC("^SISW: 0,1", response, boost::posix_time::seconds(20));
		if(!success){
			std::cout << "ERROR\t" << "Socket setup failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
		
		/*if(!modem->SendCmd("^SIST=0", response)){
			std::cout << "ERROR\t" << "SIST failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}*/
		
		//std::this_thread::sleep_for(std::chrono::seconds(3));
		if(!modem->SendCmd("^SIST=0", response)){
			std::cout << "ERROR\t" << "SIST failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
		
		success = modem->SendData(headers);
		success &= modem->SendData(message.request_messageBody);
		if(!success){
			modem->exitDataMode();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendCmd("^SISC=0", response);
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
		
		std::string data;
		if(!modem->RecvData(data, boost::posix_time::seconds(30))){
			modem->exitDataMode();
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendCmd("^SISC=0", response);
			std::cout << "ERROR\t" << "HTTP failed. executing ohShit(1)" << std::endl;
			if(!ohShit(1)) return false;
			else continue;
		}
		
		//message.host = host;
		//message.port = port;
		//message.requestLine = requestLine;
		//message.request_headers = headers;

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

		modem->SendCmd("^SISC=0", response);
		modem->SendCmd("^SICA=0,3", response);

		return true;
	}
}
/*
Modem::ErrType Modem::HttpRequest (modem_reply_t &reply,
	std::string  host,
	uint16_t     port,
	std::string  requestLine,
	std::string  headers)
{
	std::string messageBody;

	return HttpRequest(reply, host, port, requestLine, headers, messageBody);
}

Modem::ErrType Modem::HttpRequest (modem_reply_t &reply,
	std::string  host,
	uint16_t     port,
	std::string  requestLine,
	std::string  headers,
	const std::string &messageBody)
{
	Modem::ErrType result = Modem::NONE;
	bool valid;
	std::string response;
	std::string data;
    static uint8_t  timeouts = 0;
	if(modem == nullptr){
		throw(FailureException("Modem Hardware Error", "Modem is not initialized", "device_unavailable"));
	}

	if (!modem->IsOpen())
		return ErrType::SERIAL_PORT_FAIL;


	if (timeouts == 5) {
    	errors = 0;
    	timeouts = 0;

		TRACE_DEBUG("Resetting modem on timeout errors.");

		digitalWrite(28, HIGH);
		std::this_thread::sleep_for(std::chrono::seconds(6));
    	digitalWrite(28, LOW);

    	TRACE_DEBUG("Waiting for modem to boot.");

    	std::this_thread::sleep_for(std::chrono::seconds(15));

		modem->WaitURC("^", response, boost::posix_time::seconds(30));

		std::cout << "URC: " << response << std::endl;

		//how do we come to the conlusion that the modem is in an error state. No checking is done here!!
		
		return ErrType::BOOT_FAIL;
	}


	valid = modem->SendCmd("+CGDCONT?", response);
	if (!valid) {
		set_error();

		return ErrType::PDP_QUERY_ERROR;
	}

	errors = 0;
	if (response.length() > 0) {
		if (response.find("+CGDCONT: 3,\"IPV4V6\",\"NIMBLINK.GW12.VZWENTP\"") == std::string::npos) {
			valid = modem->SendCmd("+CGDCONT=3,\"IPV4V6\",\"NIMBLINK.GW12.VZWENTP\"", response);
			if (!valid)
				return ErrType::PDP_DEFINE_ERROR;
		}
	}

	bool active = false;
	valid = modem->SendCmd("^SICA?", response);
	if (valid && (response.length() > 0)) {
		if (response.find("^SICA: 3,1") !=std::string::npos) {
			active = true;
		}
	}

	if (!valid)
		return ErrType::SICA_FAIL;
	if (!active) {
		for (int retry = 0; retry < 1; retry++) {
			valid = modem->SendCmd("^SICA=1,3", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (!valid) {
				continue;
			}
			

			for (int retry = 0; retry < 6; retry++) {
				valid = modem->SendCmd("^SICA?", response);
				if (valid && (response.length() > 0)) {
					if (response.find("^SICA: 3,1") != std::string::npos) {
						active = true;
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}

			if (active)
				break;
		}
		if (!valid)
			return ErrType::SICA_FAIL;
		if (!active) {
			int signal = Signal();
			modem->SendCmd("+CMEE=2", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CEREG=2", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CCID", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CGSN", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CFUN?", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CEREG?", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CSQ", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("+CGATT?", response);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			modem->SendCmd("^SISO?", response);

			//throw(FailureException("Modem Hardware Error", "Internet connection could not be activated.", "service_unavailable"));
			if(signal != 99 && signal != 0) return Modem::NO_SIGNAL;
			else return INF3c0;
		}
	}

	valid = modem->SendCmd("^SISI?", response);
	if (!valid)
		return ErrType::INET_QUERY_FAIL;

	if (response.length() > 0) {
		if (response.find("^SISI: 0,2") == std::string::npos) {
			modem->SendCmd("^SISC=0", response);

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}


	for (int retry = 0; retry < 3; retry++) {
		if (retry) {
			modem->SendCmd("^SISC=0", response);

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

		valid = modem->SendCmd("^SISS=0,\"srvType\",\"Socket\"", response);
		if (!valid) {
			if (retry == 2) {
				std::cout << "Failed to set type of internet service." << std::endl;
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}

		valid = modem->SendCmd("^SISS=0,\"conId\",3", response);
		if (!valid) {
			if (retry == 2) {
				std::cout << "Failed to set internet service profile id." << std::endl;
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}
		valid = modem->SendCmd("^SISS=0,\"address\",\"socktcp://" + host + ":" + std::to_string(port) + ";etx\"", response);
		if (!valid) {
			if (retry == 2) {
				std::cout << "Failed to set internet service url." << std::endl;
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}

		modem->SendCmd("+CMEE=2", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CEREG=2", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CCID", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CGSN", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CFUN?", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CEREG?", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CSQ", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("+CGATT?", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		valid = modem->SendCmd("^SISO=0", response);
		if (!valid) {
			if (retry == 2) {
				std::cout << "Failed to open internet service." << std::endl;
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}

		valid = modem->WaitURC("^SISW:", response, boost::posix_time::seconds(30));
		if (!valid) {
			if (retry == 2) {
				timeouts++;

				modem->SendCmd("+CEER", response);
				modem->SendCmd("^SISO?", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				modem->SendCmd("^SISC=0", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				modem->SendCmd("^SISO?", response);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));

				std::cout << "Internet service not ready." << std::endl;
				
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}

		if (response.compare("^SISW: 0,1\r\n") != 0) {
			if (retry == 2) {
				std::cout << "Internet service not available." << std::endl;
				
				return ErrType::INET_FAIL;
			} else {
				continue;
			}
		}

		modem->SendCmd("^SISO?", response);
		break;
	}

	valid = modem->SendCmd("^SIST=0", response, boost::posix_time::seconds(12), false);
	if (!valid) {
		timeouts++;

		std::cout << "Failed to enter transparent mode." << std::endl;
		return ErrType::SIST_FAIL;
	}

#if 0
	for (int retry = 0; retry < 5; retry++) {
		valid = modem->SendCmd("+SQNSD=3,0," + std::to_string(port) + ",\"" + host + "\"", response, boost::posix_time::seconds(12), false);
		if (valid)
			break;

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	if (!valid)
		std::cout << "Failed to initiate socket connection." << std::endl;
		return ErrType::SOCKET_FAIL;
#endif

	valid = modem->SendData(requestLine);
	if (!valid) {
		std::cout << "Exiting Modem data mode (1) ..." << std::endl;
		timeouts++;

		modem->SendData("+");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendData("+");
		modem->SendData("+");
		modem->SendData("+");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("^SISC=0", response);

		std::cout << "Failed to send HTTP request line." << std::endl;
		
		return ErrType::HTTP_FAIL;
	}

	if (!headers.empty()) {
		valid = modem->SendData(headers);
		if (!valid) {
			std::cout << "Exiting Modem data mode (2) ..." << std::endl;
			timeouts++;

			modem->SendData("+");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendData("+");
			modem->SendData("+");
			modem->SendData("+");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendCmd("^SISC=0", response);

			std::cout << "Failed to send HTTP request headers." << std::endl;
			
			return ErrType::HTTP_FAIL;
		}
	}

	if (!messageBody.empty()) {
		valid = modem->SendData(messageBody);
		if (!valid) {
			std::cout << "Exiting Modem data mode (3) ..." << std::endl;
			timeouts++;

			modem->SendData("+");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendData("+");
			modem->SendData("+");
			modem->SendData("+");
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			modem->SendCmd("^SISC=0", response);

			std::cout << "Failed to send HTTP message body." << std::endl;
			
			return ErrType::HTTP_FAIL;
		}
	}

#if 1
	valid = modem->RecvData(data, boost::posix_time::seconds(30), NO_CARRIER);
	if (!valid) {
		std::cout << "Exiting Modem data mode (4) ..." << std::endl;
		timeouts++;

		modem->SendData("+");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendData("+");
		modem->SendData("+");
		modem->SendData("+");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("^SISC=0", response);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		modem->SendCmd("^SISC=0", response);

		std::cout << "Failed to receive HTTP response." << std::endl;
		
		return ErrType::HTTP_FAIL;
	}
#else

	valid = modem->SendCmd("^SISR=0,0", response);
	if (!valid){
		std::cout << "Failed to receive data." << std::endl;
		
		return ErrType::HTTP_FAIL;
	}

	valid = modem->SendCmd("^SISR=0,1000", response);
	if (!valid){
		std::cout << "Failed to receive data." << std::endl;
		
		return ErrType::HTTP_FAIL;
	}
#endif

	timeouts = 0;

	reply.host = host;
	reply.port = port;
	reply.requestLine = requestLine;
	reply.request_headers = headers;
	if (!messageBody.empty())
		reply.request_messageBody = messageBody;

	bool valid_http = data.find("HTTP/1.1 ") == 0;

	if (valid_http && (data.length() >= 20)) {
		reply.statusCode = std::stoi(data.substr(9, 3));
		reply.reasonPhrase = data.substr(13, data.find("\r\n") - 13);
		reply.response_headers = data.substr(data.find("\r\n") + 2, data.find("\r\n\r\n") - data.find("\r\n") - 2);
		size_t index = data.find("\r\n\r\n");
		if(index > 0){
			reply.response_messageBody = data.substr(index + 4);
		}
	} else {
		reply.statusCode = 502;
		reply.reasonPhrase = "Bad Gateway";
		reply.response_headers = "";
		reply.response_messageBody = data;
	}

	modem->SendCmd("^SISC=0", response);

	return reply.statusCode == 200 ? Modem::NONE : Modem::UNKNOWN;
}
*/