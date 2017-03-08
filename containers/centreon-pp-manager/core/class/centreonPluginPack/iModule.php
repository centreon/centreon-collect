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

interface CentreonPluginPack_iModule
{
    /**
     * Get the list of pluginpack
     *
     * @param int $offset - The offset of start the listing for pagination
     * @param int $limit - The number of elements for the listing for pagination
     * @param array $aFilters - The number of elements for the listing for pagination
     */
    public function getList($offset, $limit, $aFilters);
}