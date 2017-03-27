#ifndef _BASE64_H_
#define _BASE64_H_

#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>

#define BASE64_DECODE_OFFSET 43

typedef enum _base64_err_typedef{BASE64_NO_ERR = 0, INVALID_BASE64} base64_err_typedef;

base64_err_typedef base64_encode(const std::vector<uint8_t> &data, std::string &base64);

base64_err_typedef base64_decode(std::vector<uint8_t> &data, const std::string &base64);

#endif