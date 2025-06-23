#!/bin/sh
sleep  0.$(( $RANDOM % 2000))
echo -n args $* env ${NAGIOS__SERVICEVAR1} ${NAGIOS__SERVICEVAR2}
if [ "0KO" = "0${NAGIOS__SERVICEKO}" ]; then
    exit 1
fi