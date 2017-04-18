#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
cd /usr/share/centreon/www/install/steps/process
php configFileSetup.php
php installConfigurationDb.php
php installStorageDb.php
php createDbUser.php
php insertBaseConf.php
php partitionTables.php
curl -d "modules[]=centreon-license-manager&modules[]=centreon-pp-manager" http://localhost/centreon/install/steps/process/process_step8.php
rm -rf /usr/share/centreon/www/install
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
httpd -k stop
service mysql stop
