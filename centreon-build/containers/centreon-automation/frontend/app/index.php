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

// Get centreon-automation configuration file
require_once realpath(dirname(__FILE__) . '/../../centreon-automation.conf.php');

// Check if Centreon Session is available
if (!isset($oreon)) {
    exit();
}

// Check if generic-active-host is available
$genericActiveHostAvailable = false;
$hostObj = new \CentreonHost($pearDB);
if (!is_null($hostObj->getHostid('generic-active-host'))) {
    $genericActiveHostAvailable = true;
}

// Launch Smarty
$tpl = new Smarty();
$tpl = initSmartyTpl(_FRONTEND_PATH_ . 'app/', $tpl);

// Display the index page
$tpl->assign('webPath', _WEBPATH_);
$tpl->assign('genericActiveHostAvailable', $genericActiveHostAvailable);
$tpl->display('index.ihtml');
