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

// Centreon Automation base directory
$currentPath = dirname(__FILE__);


// Define path
define('_CLAPI_LIB_', _CENTREON_PATH_ . '/lib/');
define('_CLAPI_CLASS_', _CENTREON_PATH_ . '/www/class/centreon-clapi/');
set_include_path(
    implode(
        PATH_SEPARATOR,
        array(
            realpath(_CLAPI_LIB_),
            realpath(_CLAPI_CLASS_),
            get_include_path()
        )
    )
);

if (!defined('_MODULE_PATH_')) {
    define('_MODULE_PATH_', _CENTREON_PATH_ . '/www/modules/centreon-automation/');
}
define('_BACKEND_PATH_', _MODULE_PATH_ . 'backend/');
define('_BACKEND_CLASS_PATH', _BACKEND_PATH_ . 'class/');
define('_FRONTEND_PATH_', _MODULE_PATH_ . 'frontend/');
define('_WEBPATH_', './centreon/modules/centreon-automation/frontend/app/');
define('_CENTREON_CLASS_PATH_', _CENTREON_PATH_ .'/www/class/');

// Autoload
$sCentreonPath = _CENTREON_CLASS_PATH_;
$sAppPath = _BACKEND_CLASS_PATH;
$sClapiPath = _CLAPI_CLASS_;
spl_autoload_register(function ($sClass) use ($sAppPath, $sClapiPath, $sCentreonPath) {
    $sFilePath = '';

    $aExplodedClassname = explode('\\', $sClass);
    $sMainDomain = array_shift($aExplodedClassname);
    $lastKeyIndex = count($aExplodedClassname) - 1;

    // Set first character in UPPER case except for clapi
    array_walk(
        $aExplodedClassname,
        function (&$value, $key) use ($sMainDomain, $lastKeyIndex) {
            if (($key == $lastKeyIndex) && ($sMainDomain == 'CentreonClapi')) {
                $value = lcfirst($value);
            } else {
                $value = ucfirst($value);
            }
        }
    );

    // Building final file path
    $sMidFilePath = '/' . implode('/', $aExplodedClassname);
    switch ($sMainDomain) {
        case 'CentreonClapi':
            $sFilePath .= $sClapiPath . $sMidFilePath . '.class';
            break;
        case 'CentreonAutomation':
            $sFilePath .= $sAppPath . $sMidFilePath;
            break;
        default:
            $sFilePath .= $sCentreonPath . lcfirst($sMainDomain) . '.class';
            break;
    }

    $sFilePath .= '.php';

    if (file_exists($sFilePath)) {
        require_once $sFilePath;
    }
});
