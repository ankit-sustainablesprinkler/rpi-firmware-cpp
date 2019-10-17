#include  "modem.h"
#include "failure_exception.h"

#include <boost/atomic.hpp>
#include <boost/lexical_cast.hpp>
#include <wiringPi.h>
#include <thread>
#include "modem_driver.h"

using boost::lexical_cast;


using namespace boost;


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
    while (time(nullptr) - start_time < timeout) {
        if (modem->SendCmd("I", response)){
            return true;
		}
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return false;
}

modem_device_t Modem::Device()
{
	modem_device_t result;
	memset(&result, 0, sizeof(result));
	return result;
}

std::string Modem::Phone ()
{
	return "";
}

std::string Modem::IMEI ()
{
	return "";
}

int Modem::Signal ()
{
	return 0;
}

bool Modem::getInfo(modem_info_t &info)
{	
	return false;
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


bool Modem::SendCmd(const std::string &cmd, std::string &data, boost::posix_time::time_duration timeout, bool end_on_ok)
{
	modem->SendCmd(cmd, data, timeout, end_on_ok);
}

bool  Modem::Reset ()
{
	return false;
}


bool Modem::sendRequest(const std::string &headers, modem_reply_t &message)
{
	return false;
}