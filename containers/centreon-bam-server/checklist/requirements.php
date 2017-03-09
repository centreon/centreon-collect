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

$critical = false;
$warning = false;
$message = array();

/* Launch Check */
include_once "check_dep_01.php";
include_once "check_dep_02.php";
include_once "check_dep_03.php";
include_once "check_dep_04.php";
