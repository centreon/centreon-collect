#!/bin/sh

set -e
set -x

service mysql start
cd /usr/share/centreon/www/install/steps/process
php configFileSetup.php
php installConfigurationDb.php
php installStorageDb.php
php createDbUser.php
php insertBaseConf.php
php partitionTables.php
su -c "php generationCache.php" apache -s /bin/bash
rm -rf /usr/share/centreon/www/install
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
service mysql stop
