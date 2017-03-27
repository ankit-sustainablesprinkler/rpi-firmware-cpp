
#include <iostream>
#include <string>
#include "failure_exception.h"
#include "modem.h"

#define SERVER_PORT 80
#define SERVER_HOST "s-3.us"

using namespace std;

bool modemPut(Modem &modem, modem_reply_t &message);

int main()
{
	Modem modem;
	
	if(modem.Open("/dev/ttyAMA0", 115200)) {
		cout << "Connected to modem" << endl;
		modem_reply_t message;
		message.request_messageBody = "N68BAY54uVgbAAAA2+kflmMDAAD4TyITAAAA";
		try{
			
			if(modemPut(modem, message)){
				cout << message.response_messageBody << endl;
			} else {
				cout << "Failed////////////////////" << endl;
			}
			
		}catch(FailureException e){
			cout << e.what() << endl;
		}
	} else {
		
		cout << "Failed to connect to modem" << endl;
	}
	
	cout << "YUtyrewt3qw7iuyt7EI5U4T" << endl;
	return 0;
}


bool modemPut(Modem &modem, modem_reply_t &message)
{
	string requestLine =
		string("PUT ") + "http://" + SERVER_HOST + "/ HTTP/1.1\r\n";

	string headers =
		string("Host: ") + SERVER_HOST + "\r\n" +
		"User-Agent: S3-Burn/0.1 (Linux armv6l; en-US)\r\n" +
		"Content-Length: " + to_string(message.request_messageBody.length()) + "\r\n" +
		"Connection: close\r\n" +
		+ "\r\n";

	message = modem.HttpRequest(SERVER_HOST, SERVER_PORT, requestLine, headers, message.request_messageBody);
	
	return message.statusCode == 200;
}
/*
 if (uri == "/http/post") {
    	server::string_type url = jObj["url"];
    	server::string_type messageBody = jObj["messageBody"];

    	uri::uri uri(url);

    	if (!uri.is_valid())
    		return false;

    	if (uri.scheme() != "http")
    		return false;

    	uint16_t port = 80;
    	if (uri.port() != "")
    		port = lexical_cast<uint16_t>(uri.port());

    	server::string_type requestLine =
    			"POST " +
				(uri.path().empty() ? "/" : uri.path()) +
				(uri.query().empty() ? "" : "?" + uri.query()) +
				(uri.fragment().empty() ? "" : "#" + uri.fragment()) +
				" HTTP/1.1\r\n";

    	server::string_type  headers =
				"Host: " + uri.host() + "\r\n" +
				"User-Agent: Mozilla/5.0 (Linux armv6l; en-US)\r\n" +
				"Content-Length: " + lexical_cast<std::string>(messageBody.length()) + "\r\n" +
				"Connection: close\r\n" +
				+ "\r\n";

    	result = modem.HttpRequest(uri.host(), port, requestLine, headers, messageBody).dump();
    } else if (uri == "/http/put") {
    	server::string_type url = jObj["url"];
    	server::string_type messageBody = jObj["messageBody"];

    	uri::uri uri(url);

    	if (!uri.is_valid())
    		return false;

    	if (uri.scheme() != "http")
    		return false;

    	uint16_t port = 80;
    	if (uri.port() != "")
    		port = lexical_cast<uint16_t>(uri.port());

    	server::string_type requestLine =
    			"PUT " +
				(uri.path().empty() ? "/" : uri.path()) +
				(uri.query().empty() ? "" : "?" + uri.query()) +
				(uri.fragment().empty() ? "" : "#" + uri.fragment()) +
				" HTTP/1.1\r\n";

    	server::string_type  headers =
				"Host: " + uri.host() + "\r\n" +
				"User-Agent: Mozilla/5.0 (Linux armv6l; en-US)\r\n" +
				"Content-Length: " + lexical_cast<std::string>(messageBody.length()) + "\r\n" +
				"Connection: close\r\n" +
				+ "\r\n";

    	result = modem.HttpRequest(uri.host(), port, requestLine, headers, messageBody).dump();
    } else if (uri == "/http/get") {
    	server::string_type url = jObj["url"];

    	uri::uri uri(url);

    	if (!uri.is_valid())
    		return false;

    	if (uri.scheme() != "http")
    		return false;

    	uint16_t port = 80;
    	if (uri.port() != "")
    		port = lexical_cast<uint16_t>(uri.port());

    	server::string_type requestLine =
    			"GET " +
				(uri.path().empty() ? "/" : uri.path()) +
				(uri.query().empty() ? "" : "?" + uri.query()) +
				(uri.fragment().empty() ? "" : "#" + uri.fragment()) +
				" HTTP/1.1\r\n";

    	server::string_type  headers =
				"Host: " + uri.host() + "\r\n" +
				"User-Agent: Mozilla/5.0 (Linux armv6l; en-US)\r\n" +
				"Connection: close\r\n" +
				+ "\r\n";

    	result = modem.HttpRequest(uri.host(), port, requestLine, headers).dump();
    } else return false;
	 
	 */