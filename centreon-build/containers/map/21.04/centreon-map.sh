#!/bin/sh

pidfile='/var/run/map.pid'

service_start() {
    echo "mapdevscript: starting centreon-map"
    if [ \! -f "$pidfile" ] ; then
	if [ -e /etc/centreon-studio/centreon-map.conf ] ; then
            pid=`su - root -c "source /etc/centreon-studio/centreon-map.conf ; /usr/share/centreon-map-server/bin/centreon-map $RUN_ARGS $JAVA_OPTS > /dev/null 2>&1 & echo \\$!"`
	else
	    pid=`su - root -c "/usr/share/centreon-map-server/bin/centreon-map $RUN_ARGS $JAVA_OPTS > /dev/null 2>&1 & echo \\$!"`
	fi
	echo "$pid" > "$pidfile"
	echo "mapdevscript: centreon-map started"
    else
	echo "mapdevscript: centreon-map is already started"
    fi
}

service_stop() {
    echo "mapdevscript: stopping centreon-map"
    kill -TERM `cat "$pidfile"`
    sleep 2
    rm -f "$pidfile"
    echo "mapdevscript: centreon-map stopped"
}

service_restart() {
    service_stop
    service_start
}

service_reload() {
    echo "mapdevscript: reloading centreon-map"
    kill -HUP `cat "$pidfile"`
    echo "mapdevscript: centreon-map reloaded"
}

case "$1" in
    start)
	service_start
	;;

    stop)
	service_stop
	;;

    restart)
	service_restart
	;;

    reload)
	service_reload
	;;

    *)
	echo "mapdevscript: invalid arguent"
	exit 3
	;;
esac

exit 0
