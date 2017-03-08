<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

require_once ("./modules/centreon-bam-server/core/common/header.php");
require_once ("./modules/centreon-bam-server/centreon-bam-server.conf.php");
require_once ("./modules/centreon-bam-server/core/common/functions.php");

/* Pear library */
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

$pearDB = new CentreonBAM_DB();
$ba_options = new CentreonBAM_Options($pearDB, $oreon->user->user_id);
$ba_acl = new CentreonBam_Acl($pearDB, $oreon->user->user_id);

require_once ("./modules/centreon-bam-server/core/options/user/user_form.php");
