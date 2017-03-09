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
 
if (extension_loaded('apc')) {
	$critical = true;
	$message[] = array('ErrorMessage' => 'APC extension is active',
					   'Solution' => 'Please remove and/or desactivate APC extension');
}
