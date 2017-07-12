#ifndef SENSOR_GLOBAL
#define SENSOR_GLOBAL
#include "moving_average.h"

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

void sensorRead();

}

#endif