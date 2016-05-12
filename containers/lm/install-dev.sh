#!/bin/sh

set -e
set -x

sed -i "s#define('CENTREON_IMP_API_URL', '');#define('CENTREON_IMP_API_URL', 'http://middleware:3000/api');#g" /usr/share/centreon/www/modules/centreon-license-manager/centreon-license-manager.conf.php
