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
 
if (version_compare(PHP_VERSION, '5.3.2') == 0) {
	$critical = true;
	$message[] = array('ErrorMessage' => 'Your PHP Version is not supported.',
					   'Solution' => 'PHP 5.3.2 is not supported by Centreon. Please instal another PHP version from 5.1.6');
}
