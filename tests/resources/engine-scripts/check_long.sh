#!/bin/sh
rand=`shuf -i 1-15 -n 1`
sleep  $rand
echo -n args $* env ${NAGIOS__SERVICEVAR1} ${NAGIOS__SERVICEVAR2}
if [ "0KO" = "0${NAGIOS__SERVICEKO}" ]; then
    exit 1
fi