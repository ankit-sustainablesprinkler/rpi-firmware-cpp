#!/bin/bash

#Output is piped to /var/log/ota.log

OTA_SYNC="/tmp/ota-sync"
NEW_FIRMWARE="/home/pi/firmware/main/s3-main-new"
FIRMWARE="/home/pi/firmware/main/s3-main"
ETC_DIR="/home/pi/firmware/main/etc/"
OLD_ETC_DIR="/home/pi/firmware/main/old-etc/"
OLD_FIRMWARE="/home/pi/firmware/main/s3-main-old"

if [ $1 = "update" ] ; then
	if [ -f $NEW_FIRMWARE ] ; then
		if [ -f $OTA_SYNC ] ; then
			echo Updating...
			echo `mv $FIRMWARE $OLD_FIRMWARE`
			echo `mv $NEW_FIRMWARE $FIRMWARE`
			echo Removing old configs
			FILES=$ETC_DIR\*
			echo `rm -r $OLD_ETC_DIR`
			mkdir $OLD_ETC_DIR
			echo `mv $FILES $OLD_ETC_DIR`
			echo `rm $FILES`
			echo Restarting Service
			echo `/usr/sbin/service s3-main restart`
		else
			echo Not ready to update.
			FIRMWARE_TIME=`date +%s -r $FIRMWARE`
			CURRENT_TIME=`date +%s`
			ELAPSED=$(($CURRENT_TIME - $FIRMWARE_TIME))
			if [ $ELAPSED -gt 86400 ] ; then
				echo Updating anyways
				echo `mv $FIRMWARE $OLD_FIRMWARE`
				echo `mv $NEW_FIRMWARE $FIRMWARE`
				echo Removing old configs
				FILES=$ETC_DIR\*
				echo `rm -r $OLD_ETC_DIR`
				mkdir $OLD_ETC_DIR
				echo `mv $FILES $OLD_ETC_DIR`
				echo `rm $FILES`
				echo Restarting Service
				echo `/usr/sbin/service s3-main restart`
			fi
		fi
	else
		echo Nothing to update
	fi
elif [ $1 = "rollback" ] ; then
	if [ -f $OLD_FIRMWARE ] ; then
		echo Rolling back...
		echo `mv $FIRMWARE $NEW_FIRMWARE`
		echo `mv $OLD_FIRMWARE $FIRMWARE`
		FILES=$ETC_DIR\*
		echo `rm $FILES`
		OLD_FILES=$OLD_ETC_DIR\*
		echo `mv $OLD_FILES $ETC_DIR`
		echo `/usr/sbin/service s3-main restart`
	else
		echo Nothing to rollback
	fi
fi