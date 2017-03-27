//g++ -std=c++11 -o test test.cpp -llcm

#include <iostream>

#include <lcm/lcm-cpp.hpp>
#include "sensor_t.hpp"

using namespace std;

class Handler 
{
	public:
		~Handler() {}

		void handleMessage(const lcm::ReceiveBuffer* rbuf,
				const std::string& chan, 
				const sensor_t* msg)
		{
			cout << "current:  " << msg->solenoid_current << "  voltage:  " << msg->solenoid_voltage << "  flow:  " << msg->flow << endl;
		}
};

int main(int argc, char** argv)
{
	lcm::LCM lcm;

	if(!lcm.good())
		return 1;

	Handler handlerObject;
	lcm.subscribe("SENSOR", &Handler::handleMessage, &handlerObject);

	while(0 == lcm.handle());

	return 0;
}