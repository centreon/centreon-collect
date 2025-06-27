#!/bin/sh
rand=`shuf -i 1-1000 -n 1`
sleep  0.$(( $rand % 1000))
echo -n args $* env ${NAGIOS__SERVICEVAR1} ${NAGIOS__SERVICEVAR2}
if [ "0KO" = "0${NAGIOS__SERVICEKO}" ]; then
    exit 1
fi