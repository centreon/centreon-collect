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
namespace CentreonPluginPackManager\Installation;

/**
 *
 * @author lionel
 */
interface OperationManagerInterface
{
    /**
     * 
     * @param array $pluginPackList
     */
    public function launchOperation($pluginPackList);
}
