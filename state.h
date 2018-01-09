#ifndef STATE_H
#define STATE_H


#include "schedule.h"
#define STATE_FILE_PREFIX "/home/pi/firmware/main/etc/"
#define STATE_FILE STATE_FILE_PREFIX "state"
#define FEEDBACK_FILE STATE_FILE_PREFIX "feedback"
#define FLOW_FEEDBACK_FILE STATE_FILE_PREFIX "flow_feedback"
#define ALERT_FEEDBACK_FILE STATE_FILE_PREFIX "alert_feedback"
#define FLOW_CAL_RESULTS_FILE STATE_FILE_PREFIX "calibration_results"

namespace bin_protocol{
	class Feedback;
}

struct s3state_var_t {
	time_t last_heartbeat_time = 0;
	time_t current_feedback_time = 0;
	time_t previous_feedback_time = 0;
	time_t current_flow_feedback_time = 0;
	time_t previous_flow_feedback_time = 0;
	run_state_t previous_state;
	int current_feedback = 0;
	int current_flow_feedback = 0;
	bool voltage_state = false;
	bool current_state = false;
	bool voltage_state_prev = false;
	bool current_state_prev = false;
	bool calibration_complete = false;

	//sensor state
	bool blocked_pump_detected;
	time_t blocked_pump_time;
	bool unscheduled_flow;
	time_t unscheduled_flow_time;
	bool low_flow;
	time_t low_flow_time;
	bool high_flow;
	time_t high_flow_time;
	bool very_high_flow;
	time_t very_high_flow_time;
	int ovc_trigger_count;
};

struct s3state_t {
	s3state_var_t var;
	bin_protocol::Feedback feedback[2]; // keep two feedback logs so we can continue logging while waiting for upload.
	bin_protocol::FlowFeedback flow_feedback[2];
	bin_protocol::AlertFeedback alert_feedback;
	bin_protocol::CalibrationResult cal_result;
};

bool state_getState(s3state_t &state);
bool state_saveState(const s3state_t &state);

#endif 