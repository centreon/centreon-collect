<?php
/**
 * Copyright 2016 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Define the base url for IMP API */
$preprod = getenv('CENTREON_IMP_PREPROD');
if (($preprod !== FALSE) && ($preprod == 'HELLOWORLD')) {
    define('CENTREON_IMP_API_URL', 'https://ppd-api.imp.centreon.com/api');
}
else {
    define('CENTREON_IMP_API_URL', 'http://api.imp.centreon.com:3000/api');
}

/* Centreon Automation base directory */
$currentPath = dirname(__FILE__);
/* Load Installation configuration file */
$installationConfigurationFile = realpath(dirname(__FILE__) . '/../../../config/centreon.config.php');
require_once $installationConfigurationFile;

/* Directory for put license files */
define('_CENTREON_LICENSE_PATH_', realpath(_CENTREON_ETC_ . '/license.d'));

define('_MODULE_PATH_', _CENTREON_PATH_ . '/www/modules/centreon-license-manager/');
define('_FRONTEND_PATH_', _MODULE_PATH_ . 'frontend/');
define('_WEBPATH_', './modules/centreon-license-manager/frontend/app/');
