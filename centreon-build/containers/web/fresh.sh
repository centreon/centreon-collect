#!/bin/sh

set -e
set -x

service mysql start
httpd -k start
cd /usr/share/centreon/www/install/steps/process
cat ../../../../autoinstall.php installConfigurationDb.php | php
cat ../../../../autoinstall.php installStorageDb.php | php
cat ../../../../autoinstall.php installUtilsDb.php | php
cat ../../../../autoinstall.php createDbUser.php | php
cat ../../../../autoinstall.php insertBaseConf.php | php
cat ../../../../autoinstall.php configFileSetup.php | php
rm -rf /usr/share/centreon/www/install
mysql -e "GRANT ALL ON *.* to root@'%' IDENTIFIED BY 'centreon'"
mkdir -m 777 /etc/centreon/license.d
centreon -d -u admin -p centreon -a POLLERGENERATE -v 1
centreon -d -u admin -p centreon -a CFGMOVE -v 1
httpd -k stop
service mysql stop
