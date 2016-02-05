#!/bin/sh

export NODE_PATH=/usr/local/lib/node_modules
service mysql start
service httpd start
cd /usr/share/centreon/www/install/steps/process
cat ../../../../autoinstall.php installConfigurationDb.php | php
cat ../../../../autoinstall.php installStorageDb.php | php
cat ../../../../autoinstall.php installUtilsDb.php | php
cat ../../../../autoinstall.php createDbUser.php | php
cat ../../../../autoinstall.php insertBaseConf.php | php
cat ../../../../autoinstall.php configFileSetup.php | php
rm -rf /usr/share/centreon/www/install
cd
echo
bash
