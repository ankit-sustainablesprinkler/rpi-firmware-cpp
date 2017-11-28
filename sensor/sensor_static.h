#ifndef SENSOR_GLOBAL
#define SENSOR_GLOBAL
#include "moving_average.h"
#include "state.h"

#include <ctime>


/** temporary static interface to the sensor functions and state

*/
namespace sensor{
//functions

void sensorInit();
void resetFault();
void resetCurrent();
void resetVoltage();
void resetFlow();

void sensorRead(s3state_t &state, bin_protocol::Schedule &schedule, bin_protocol::Config &config, bin_protocol::FlowConfiguration &FlowConfiguration);

}

#endif