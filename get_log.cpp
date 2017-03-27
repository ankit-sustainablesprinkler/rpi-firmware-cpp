#include <iostream>
#include <lcm/lcm-cpp.hpp>
#include "Message.hpp"

using namespace std;

class Handler 
{
public:
	~Handler() {}

	void handleMessage(const lcm::ReceiveBuffer* rbuf,	const std::string& chan, const Message* msg)
	{
		cout << msg->msg << endl;
	}
};

int main()
{
	lcm::LCM lcm;
	Handler handlerObject;
	
	lcm.subscribe("LOGGER", &Handler::handleMessage, &handlerObject);
	
	while(true){
		lcm.handle();		
	}
	
	return 0;
}