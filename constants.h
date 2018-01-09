#ifndef CONSTANTS_H
#define CONSTANTS_H

#define FIRMWARE_VERSION 9

#define PIN_WDT_RESET 2
#define PIN_RELAY 3
#define PIN_RESET 7
#define PIN_SYS_FAULT 4
#define PIN_STATUS 1

#define DEBUG_MODEM

#define USE_DEBUG_SERVER
//#define USE_OFFICE_SERVER

#ifdef USE_DEBUG_SERVER
#define SERVER_HOST "staging.s-3.us" //34.201.59.209" //"ec2-34-201-91-4.compute-1.amazonaws.com"//"s-3.us" 
#define SERVER_PORT 8080 //8118
#define SERVER_PATH "/api/bin"
#elif defined USE_OFFICE_SERVER
#define SERVER_HOST "73.139.249.14"
#define SERVER_PORT 80
#define SERVER_PATH "/~s3staging/3s/api/bin"
#else
#define SERVER_HOST "s-3.us" 
#define SERVER_PORT 80
#define SERVER_PATH ""
#endif

//file definitions
#define FILE_PREFIX "/home/pi/firmware/main/logs/"
#define SYS_FILE_PREFIX "/home/pi/firmware/main/etc/"
#define ROOT_PREFIX "/home/pi/firmware/main/"
#define CONFIG_FILE SYS_FILE_PREFIX "config"
#define FLOW_CONFIG_FILE SYS_FILE_PREFIX "flow_config"
#define SCHEDULE_FILE SYS_FILE_PREFIX "schedule"
#define CALIBRATION_FILE SYS_FILE_PREFIX "calibration"
#define FIRMWARE_FILE ROOT_PREFIX "s3-main-new"
#define HISTORY_FILE FILE_PREFIX "history.txt"
#define HEARTBEAT_FAIL_COUNTER FILE_PREFIX "fail_count.txt"
#define MODEM_LOG FILE_PREFIX "modem_log.txt"
#define SYSTEM_LOG FILE_PREFIX "system_log.txt"
#define PID_FILE "/tmp/s3-schedule.pid"
#define OTA_SYNC_FILE "/tmp/ota-sync"

#define PIN_MODEM_DTR 25
#define PIN_MODEM_RTS 27
#define PIN_MODEM_ON 28

#define HEARTBEAT_MIN_PERIOD 120

#endif