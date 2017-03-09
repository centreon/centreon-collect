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
 
if (version_compare(PHP_VERSION, '5.3.0') <= 0) {
	/* CheckZend Optimizer */
	if(!function_exists('zend_optimizer_version')) {
		$critical = true;
		$message[] = array('ErrorMessage' => 'Zend Optimizer is not installed.',
						   'Solution' => 'Please download and install Zend Optimizer');
	}
} else {
	/* CheckZend Guard */
	if (!function_exists('zend_loader_version')) {
		$critical = true;
		$message[] = array('ErrorMessage' => 'Zend Guard is not installed.',
						   'Solution' => 'Please download and install Zend Guard');
	}
}

