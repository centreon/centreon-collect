#!/bin/sh

pidfile='/var/run/mariadb/mariadb.pid'

service_start() {
    # Launch database.
    echo "mysqldevscript: starting mysql"
    su - mysql -s /bin/sh -c "/usr/libexec/mysqld --basedir=/usr" &

    # Wait for the database to be up and running.
    while true ; do
      timeout 10 mysql -e 'SELECT 1'
      retval=$?
      if [ "$retval" = 0 ] ; then
        break ;
      else
        echo 'DB server is not yet responding.'
        sleep 1
      fi
    done

    # Good to go.
    echo "mysqldevscript: mysql started"
}

service_stop() {
    echo "mysqldevscript: stopping mysql"
    while [ "$?" -eq 0 ] ; do
        kill -0 `cat "$pidfile"`
        rm -f "$pidfile"
    done
    echo "mysqldevscript: mysql stopped"
}

service_restart() {
    service_stop
    service_start
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

    *)
        echo "mysqldevscript: invalid argument"
        exit 3
        ;;
esac

exit 0
