#!/bin/sh

ls -R /usr/share/centreon/www/modules/centreon-license-manager/
cd /usr/share/centreon/www/modules/centreon-license-manager/frontend/app
npm install
gulp
