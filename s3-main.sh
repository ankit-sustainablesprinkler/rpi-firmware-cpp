#!/bin/bash

### BEGIN INIT INFO
# Provides:          s3-main
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: s3 main launch
### END INIT INFO


# Must be a valid filename
NAME=s3_main
PIDFILE=/var/run/$NAME.pid
#This is the command to be run, give the full pathname
DAEMON=/home/pi/firmware/main/s3-main
DAEMON_USER=root
LOGGER=/home/pi/firmware/main/logger

start() {
    echo "Starting S3-main"
    ifconfig lo multicast
    route add -net 224.0.0.0 netmask 240.0.0.0 dev lo

    start_daemon

    echo "Retrieving updated information from server"
  #  python /root/3s/firmware/run_remote.py              >> /var/log/3s/sysboot.log
  #  python /root/3s/firmware/run_schedule.py            >> /var/log/3s/sysboot.log
}

stop() {
    echo "Stopping S3 System"
    stop_daemon
}

start_daemon() {
    # nohup does not work well on ubuntu... dies after ~24 hours
    # nohup /root/3s/firmware/bin/webserver 0 80 /dev/ttyAMA0 < /dev/null > /var/log/3s/s3-sensor.log 2>&1 &
    echo "starting s3-main daemon..."
    start-stop-daemon --start --quiet --chuid $DAEMON_USER    \
        --make-pidfile --pidfile $PIDFILE --background       \
        --startas /bin/bash -- -c "exec $DAEMON | $LOGGER"
}
stop_daemon(){
    echo "stopping schedule daemon..."
    kill -9 `pidof s3-main`
    sleep 2
}


case "$1" in
    start)
       start
       ;;
    stop)
       stop
       ;;
    restart)
       stop
       start
       ;;
    status)
       # code to check status of app comes here
       # example: status program_name
       ;;
    *)
       echo "Usage: $0 {start|stop|status|restart}"
esac

exit 0
