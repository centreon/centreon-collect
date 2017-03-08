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


class CentreonBam_License
{
    /**
     * Checks license validty
     *
     * @return bool
     */
    public function checkValidity($licenseFile) {
        if (function_exists("zend_loader_file_encoded") && zend_loader_file_encoded() == true) {
			$licenseValidity = zend_loader_install_license($licenseFile, true);
			if ($licenseValidity == false) {
				return false;
            }
			return true;
        }
        return true;
    }
}
