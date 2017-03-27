#ifndef CONSTANTS_H
#define CONSTANTS_H

#define FIRMWARE_VERSION 2

#define PIN_RELAY 3
#define PIN_RESET 7
#define PIN_SYS_FAULT 4


#define SERVER_PORT 80
#define SERVER_HOST "s-3.us"

//file definitions
#define FILE_PREFIX "/home/pi/firmware/main/logs/"
#define CONFIG_FILE FILE_PREFIX "config.dat"
#define SCHEDULE_FILE FILE_PREFIX "schedule.dat"
#define HISTORY_FILE FILE_PREFIX "history.txt"
#define HEARTBEAT_FAIL_COUNTER FILE_PREFIX "fail_count.txt"
#define MODEM_LOG FILE_PREFIX "modem_log.txt"
#define SYSTEM_LOG FILE_PREFIX "system_log.txt"

#define PIN_MODEM_DTR 25
#define PIN_MODEM_RTS 27
#define PIN_MODEM_ON 28

#define HEARTBEAT_MIN_PERIOD 120

#endif