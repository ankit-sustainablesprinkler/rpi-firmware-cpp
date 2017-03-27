#include "base64.h"



static const char base64_encode_str[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64_decode_str[80] = {62,255,255,255,63,52,53,54,55,56,57,58,59,60,61,255,\
											 255,255,0,255,255,255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,\
											 10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,\
											 255,255,255,255,255,255,26,27,28,29,30,31,32,33,34,35,\
											 36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};

char _base64_decode_char(char val){
	if(val >= BASE64_DECODE_OFFSET && val < BASE64_DECODE_OFFSET + 80){
		return base64_decode_str[val - BASE64_DECODE_OFFSET] != 255? base64_decode_str[val - BASE64_DECODE_OFFSET] : 0;
	}
}

base64_err_typedef base64_encode(const std::vector<uint8_t> &data, std::string &base64)
{
	base64_err_typedef error = BASE64_NO_ERR;
	base64 = "";
	int size = -(-((int)data.size()) / 3) * 4;
	base64.reserve(size);
	int i = 0;
	for(; i < data.size() / 3; i++){
		base64 += base64_encode_str[data[i*3]>>2];
		base64 += base64_encode_str[(data[i*3+1]>>4 | data[i*3]<<4) & 0x3f];
		base64 += base64_encode_str[(data[i*3+2]>>6 | data[i*3+1]<<2) & 0x3f];
		base64 += base64_encode_str[data[i*3+2] & 0x3f];
	}

	switch(data.size()%3){
	case 1:
		base64 += base64_encode_str[data[i*3]>>2];
		base64 += base64_encode_str[data[i*3]<<4 & 0x3f];
		base64 += '=';
		base64 += '=';
		break;
	case 2:
		base64 += base64_encode_str[data[i*3]>>2];
		base64 += base64_encode_str[(data[i*3+1]>>4 | data[i*3]<<4) & 0x3f];
		base64 += base64_encode_str[data[i*3+1]<<2 & 0x3f];
		base64 += '=';
		break;
	}
	return error;
}

base64_err_typedef base64_decode(std::vector<uint8_t> &data, const std::string &base64)
{
	std::cout << base64 << std::endl;
	base64_err_typedef error = BASE64_NO_ERR;
	if(base64.size()%4 || !base64.size()){
		std::cout << "ERROR " << base64.size() << std::endl;
		error = INVALID_BASE64;
	} else{
		data.reserve(base64.size()/4*3);
		int i = 0;
		for(; i < base64.size()/4 - 1; i++){
			data.push_back((char)((_base64_decode_char(base64[i*4]) << 2) | (_base64_decode_char(base64[i*4 + 1]) >> 4)));
			data.push_back((char)((_base64_decode_char(base64[i*4 + 1]) << 4) | (_base64_decode_char(base64[i*4 + 2]) >> 2)));
			data.push_back((char)((_base64_decode_char(base64[i*4 + 2]) << 6) | (_base64_decode_char(base64[i*4+3]))));
		}
		auto iter = base64.end() - 2;
		data.push_back((char)((_base64_decode_char(base64[i*4]) << 2) | (_base64_decode_char(base64[i*4 + 1]) >> 4)));
		if(*iter++ != '=') {
			data.push_back((char)((_base64_decode_char(base64[i*4 + 1]) << 4) | (_base64_decode_char(base64[i*4 + 2]) >> 2)));
		}
		if(*iter != '='){
			data.push_back((char)((_base64_decode_char(base64[i*4 + 2]) << 6) | (_base64_decode_char(base64[i*4+3]))));
		}
	}
	std::cout << "TTTTTTTTTTT " << (error == BASE64_NO_ERR) << std::endl;
	return error;
}
