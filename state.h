#ifndef STATE_H
#define STATE_H


#include "schedule.h"
#define STATE_FILE_PREFIX "/home/pi/firmware/main/etc/"
#define STATE_FILE STATE_FILE_PREFIX "state"
#define FEEDBACK_FILE STATE_FILE_PREFIX "feedback"

namespace bin_protocol{
	class Feedback;
}

struct s3state_var_t {
	time_t last_heartbeat_time = 0;
	time_t current_feedback_time = 0;
	time_t previous_feedback_time = 0;
	run_state_t previous_state;
	int current_feedback = 0;
	bool voltage_state = false;
	bool current_state = false;
	bool voltage_state_prev = false;
	bool current_state_prev = false;
};

struct s3state_t {
	s3state_var_t var;
	bin_protocol::Feedback feedback[2]; // keep two feedback logs so we can continue logging while waiting for upload.
};

bool state_getState(s3state_t &state);
bool state_saveState(const s3state_t &state);

#endif 