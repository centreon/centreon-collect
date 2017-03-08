<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonPluginPackManager;

class License
{
    public static function isValid()
    {
        return file_exists(_CENTREON_ETC_ . '/license.d/centreon-pp-manager.license');
    }
}
