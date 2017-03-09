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

if (!defined('_MODULE_PATH_')) {
    define('_MODULE_PATH_', _CENTREON_PATH_ . '/www/modules/centreon-poller-display-central/');
}

// Autoload
$classDirectory = _MODULE_PATH_ . '/core/class/';
spl_autoload_register(function ($className) use ($classDirectory) {
    $explodedClassName = explode('\\', $className);
    array_shift($explodedClassName);

    $classFileName = array_pop($explodedClassName);
    $explodedClassName = array_map('lcfirst', $explodedClassName);

    $classPath = $classDirectory . implode('/', $explodedClassName) . '/' . $classFileName . '.php';

    if (file_exists($classPath)) {
        require_once $classPath;
    }
});
