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

/* Get centreon-license-manager configuration file */
require_once realpath(dirname(__FILE__) . '/../../centreon-license-manager.conf.php');

/* Check if Centreon Session is available */
if (!isset($centreon)) {
    exit();
}

$locale = substr($centreon->user->get_lang(), 0, 2);

/* Launch Smarty */
$tpl = new Smarty();
$tpl = initSmartyTpl(_FRONTEND_PATH_ . 'app/', $tpl);
/* Display the index page */
$tpl->assign('webPath', _WEBPATH_);
$tpl->assign('locale', $locale);
$tpl->display('index.ihtml');
