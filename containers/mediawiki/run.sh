#!/bin/sh

set -x

sed -i -e 's|http://mediawiki|http://'`hostname --ip-address | cut -d ' ' -f 1`'|g' /var/www/html/LocalSettings.php
chmod a+w /var/www/html/config
service mysql start
echo "CREATE USER 'centreon'@'%' IDENTIFIED BY 'centreon'; GRANT ALL PRIVILEGES ON *.* TO 'centreon'@'%' WITH GRANT OPTION;CREATE USER 'centreon'@'localhost' IDENTIFIED BY 'centreon'; GRANT ALL PRIVILEGES ON *.* TO 'centreon'@'localhost' WITH GRANT OPTION;" | mysql --user=root
cat /usr/share/wikidb.db | mysql --user=root
httpd -k start
tailf /var/log/httpd/error_log
