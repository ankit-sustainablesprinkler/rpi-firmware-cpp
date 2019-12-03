#include <lcm/lcm-cpp.hpp>
#include "cmd_key_t.hpp"
#include "message_t.hpp"
#include <iostream>

using namespace std;

class Handler 
{
public:
	~Handler() {}

	void handleMessage(const lcm::ReceiveBuffer* rbuf,	const std::string& chan, const message_t* msg)
	{
		cout << msg->message << endl;
	}
};


int main(int argc, char **argv)
{
	if(argc <3){
		cout << "Incomplete key value pair" << endl;
 		return 2;
	}

	Handler handlerObject;
	cmd_key_t key_value;
	lcm::LCM lcm;
	if(!lcm.good()){
		cout << "HERE" << endl;
		return 1;
	}	

	lcm.subscribe("CMD_ERR", &Handler::handleMessage, &handlerObject);

	key_value.key = argv[1];
	key_value.value = argv[2];

	
	lcm.publish("CMD", &key_value);
	lcm.handleTimeout(1000);
	
	return 0;
}
