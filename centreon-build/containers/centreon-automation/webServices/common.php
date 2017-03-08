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

require_once realpath(dirname(__FILE__) . '/../centreon-automation.conf.php');

require_once _CENTREON_PATH_ . "/www/api/class/webService.class.php";
require_once _CENTREON_PATH_ . "/www/api/exceptions.php";

require_once _CLAPI_CLASS_ . "/centreon.Config.Poller.class.php";
require_once _CLAPI_LIB_ . "/Centreon/Db/Manager/Manager.php";

global $conf_centreon;
$dbConfig = array();
$dbConfig['host'] = $conf_centreon['hostCentreon'];
$dbConfig['username'] = $conf_centreon['user'];
$dbConfig['password'] = $conf_centreon['password'];
$dbConfig['dbname'] = $conf_centreon['db'];
$clapiDb = \Centreon_Db_Manager::factory('centreon', 'pdo_mysql', $dbConfig);
$dbConfig['dbname'] = $conf_centreon['dbcstg'];
$clapiDbMonitoring = \Centreon_Db_Manager::factory('storage', 'pdo_mysql', $dbConfig);
