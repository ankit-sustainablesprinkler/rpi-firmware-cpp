#ifndef _SENSOR_H_
#define _SENSOR_H_

#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <mutex>
#include <iostream>
#include <stddef.h>
#include <stdexcept>
#include <string.h>
#include <mutex>
#include <stdint.h>
#include <stddef.h>
#include <vector>

#include "crc16.h"

class SensorState
{
public:
	float flow;
	float solenoid_voltage;
	float solenoid_current;
	float transformer_voltage;
};

typedef  struct  Status_t {
	uint8_t   HardwareID;
	uint8_t   FirwareID;					/* 0 = Bootloader, 1 = Main */
	uint16_t  FirmwareVersion;
	uint16_t  FirmwareRevision;
	struct {
		uint16_t  Reserved : 14;
		uint16_t  EraseFlash : 1;
		uint16_t  WriteFlash : 1;
	} ErrorFlags;
} Status_t;

typedef  enum  CommandCode_t {
    Cmd_GetStatus                      = 0x00,

	Cmd_EraseFlash                     = 0x10,					/* Supported in bootloader only.						*/
    Cmd_WriteFlash                     = 0x11,					/* Supported in bootloader only.						*/

	Cmd_StartMain                      = 0x20,
    Cmd_StartBootloader                = 0x21,

    Cmd_GetVersionBootloader           = 0x30,
    Cmd_GetVersionMain                 = 0x31,
} CommandCode_t;

typedef  enum  RespStatus_t {
	RespIdle             = 0xFF,
	RespInvalidHdr       = 0x7F,
	RespInvalidCmd       = 0x7E,
	RespInvalidParamsLen = 0x7D,
	RespFrameLenExceeded = 0x7C,
	RespFrameLenShort    = 0x7B,
	RespRecvOverrun      = 0x7A,
	RespSendOverrun      = 0x79,
	RespLenExceeded      = 0x78,
	RespCrcError         = 0x70,
	RespHeader           = 0x1A,
} RespStatus_t;

void sensorRun(SensorState *state);
void sensorSetup();

bool meterGetValues(float  &volt_ac, float &volt_solenoid, float &curr_solenoid);

bool flow_get (std::vector<uint16_t>  &rate);
bool flow_getStatus (Status_t  &status);
bool flow_startBootloader(void);
bool flow_startMain(void);
bool flow_eraseFlash(uint32_t  address, size_t  len);
bool flow_writeFlash(uint32_t  address, bool last_xfer, const  uint8_t  *data, size_t  len);

#endif