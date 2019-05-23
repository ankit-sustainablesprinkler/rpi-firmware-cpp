#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include <string>
#include <stdint.h>

#include "bin_protocol.h"
#include "base64.h"
#include "constants.h"


uint32_t getSerialNumber();

float getTemperature();

uint32_t getUpTime();

bool getConfig(bin_protocol::Config &config);
bool saveConfig(const bin_protocol::Config &config);

bool getFlowConfig(bin_protocol::FlowConfiguration &flow_config);
bool saveFlowConfig(const bin_protocol::FlowConfiguration &flow_config);

bool getSchedule(bin_protocol::Schedule &schedule);
bool saveSchedule(const bin_protocol::Schedule &schedule);

bool getCalibration(bin_protocol::CalibrationSetup &calibration);
bool saveCalibration(const bin_protocol::CalibrationSetup &calibration);

int getHeartbeatFailCount();
void setHeartbeatFailCount(int count);
int incrementHeartbeatFail();

bin_protocol::Header getHeader(bin_protocol::Type type);

bin_protocol::Heartbeat getHeartbeat(std::string extra_content="");

bool getRelayState();

void openRelay();

void closeRelay();

bool isLogReady();

void rebootSystem();

std::string runCommand(std::string cmd);

bool handleHBResponse(const std::string &response, std::vector<int> &handled_types, std::vector<int> &unhandled_types);

#endif