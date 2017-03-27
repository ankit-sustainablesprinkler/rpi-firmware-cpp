#include <iostream>
#include <vector>
#include <string>

#include "base64.h"
#include "bin_protocol.h"

using namespace std;

int main(int argc, char **argv)
{
	cout << argc << endl;
	string test(argv[1]);// = "Y0gBAQymtVgbAAAA2+kflqQVAAAyTyUZAAAA";
	vector<uint8_t> data;
	cout << "shfdhfsh  " << argv[1] << endl;
	base64_decode(data, test);
	
	cout << "uuuuu" << endl;
	cout << bin_protocol::getTypefromBinary(data) << endl;
	
	bin_protocol::Heartbeat heartbeat;
	cout << heartbeat.fromBinary(data) << endl;
	
	cout << heartbeat.header.ID << endl;
	
	
	
	return 0;
}