#ifndef _CRC_H_
#define _CRC_H_
#include <stdint.h>
#include <vector>
uint16_t  CRC16 (const std::vector<uint8_t> &data, uint16_t  init = 0);
#endif // _CRC_H_
