#ifndef SENSOR_GLOBAL
#define SENSOR_GLOBAL

#include <ctime>


/** temporary static interface to the sensor functions and state

*/
namespace Sensor{

//static variables
static bool voltage_state = false, voltage_state_prev = false, current_state = false, current_state_prev = false;
static time_t last_log = time(nullptr);
static time_t voltage_start = 0, current_start = 0;

//static functions

static void init();
static void resetFault();
static void resetCurrent();
static void resetVoltage();
static void resetFlow();

static void sensorRead();

}

#endif