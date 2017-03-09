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
$preprod = getenv('CENTREON_IMP_SERVER');

if (($preprod !== FALSE) && ($preprod == 'HELLOWORLD')) {
    define('CENTREON_IMP_API_URL', 'https://ppd-api.imp.centreon.com/api');
} else if (($preprod !== FALSE) && preg_match('/HELLOWORLD\:(.+)/', $preprod, $matches)) {
    define('CENTREON_IMP_API_URL', $matches[1]);
} else {
    define('CENTREON_IMP_API_URL', 'https://api.imp.centreon.com/api');
}

if (!defined('MODULE_PATH')) {
    define('MODULE_PATH', _CENTREON_PATH_ . '/www/modules/centreon-pp-manager/');
}

define('EMS_PP_PATH', '/usr/share/centreon-packs/');

define('PPM_IMG_PATH', _CENTREON_PATH_ . '/www/img/ppm/');

// Autoload for Installation classes
$sAppPath = MODULE_PATH . 'core/class/centreonPluginPack/Installation';
spl_autoload_register(function ($sClass) use ($sAppPath) {
    $sFilePath = $sAppPath;
    $aExplodedClassname = explode('\\', $sClass);
    $sCentreonPluginPack = array_shift($aExplodedClassname);
    $sInstallation = array_shift($aExplodedClassname);

    $sFilePath .= '/' . implode('/', $aExplodedClassname);
    $sFilePath .= '.php';

    if (file_exists($sFilePath)) {
        require_once $sFilePath;
    }
});
