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

namespace CentreonPluginPackManager\Installation\Provider;

interface ProviderInterface
{
    /**
     *
     * @param array $pluginPackInfo
     */
    public function getPluginPackJsonFile($pluginPackInfo, $action = '');

    /**
     *
     * @param array $pluginPackInfo
     */
    public function getPluginPackJsonFileOfLastVersion($pluginPackInfo);
}
