set -e
set -x

mkdir -p /var/log/centreon-engine /var/lib/centreon-engine/rw /var/log/centreon-broker /var/lib/centreon-broker /var/lib/centreon /var/lib/centreon/centplugins
chown -R centreon-engine: /var/log/centreon-engine /var/lib/centreon-engine /var/log/centreon-broker /var/lib/centreon-broker /var/lib/centreon /var/lib/centreon/centplugins
chmod 777  /var/lib/centreon/centplugins

echo "engine config:"
ls -l /etc/centreon-engine

echo "broker config:"
ls -l /etc/centreon-broker

su centreon-engine -c "/usr/sbin/centengine /etc/centreon-engine/centengine.cfg"
