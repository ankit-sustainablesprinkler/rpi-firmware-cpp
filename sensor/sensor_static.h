#ifndef SENSOR_GLOBAL
#define SENSOR_GLOBAL
#include "moving_average.h"

#include <ctime>


/** temporary static interface to the sensor functions and state

*/
namespace sensor{

//static variables
static bool voltage_state = false, voltage_state_prev = false, current_state = false, current_state_prev = false;
static time_t last_log = time(nullptr);
static time_t voltage_start = 0, current_start = 0;
static time_t volt_on_time = 0, curr_on_time = 0, flow_on_time = 0;





//static float voltage, solenoid_voltage, solenoid_current, frequency;
//static int voltage_count, current_count, frequency_count;

static MovingAverage<float> flow_average(60);
static MovingAverage<float> current_average(60);
static MovingAverage<float> voltage_average(60);
static MovingAverage<float> transformer_voltage_average(120);

//functions

void sensorInit();
void resetFault();
void resetCurrent();
void resetVoltage();
void resetFlow();

void sensorRead();

}

#endif