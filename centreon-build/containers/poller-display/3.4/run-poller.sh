#!/bin/sh

# Get initial configuration export time.
initial_export_time=`stat -c '%Y' /etc/centreon-broker/watchdog.xml`

# Run SSH service.
/usr/sbin/sshd -D &

# Wait for configuration to be exported from the central.
current_export_time=$initial_export_time
while [ "$current_export_time" -le "$initial_export_time" ] ; do
  sleep 1
  current_export_time=`stat -c '%Y' /etc/centreon-broker/watchdog.xml`
done

# Run database synchronization.
/usr/share/centreon/cron/centreon-poller-display-sync.sh
