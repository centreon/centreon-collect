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

class ProviderFactory
{
    /**
     *  Build a new provider, adapted to the context.
     *
     *  @return New provider, either using on-disk files or the IMP server.
     */
    public static function newProvider($impApi = null)
    {
        if (\CentreonPluginPackManager\License::isValid()) {
            $provider = new \CentreonPluginPackManager\Installation\Provider\EMS();
        } else {
            $provider = new \CentreonPluginPackManager\Installation\Provider\IMP($impApi);
        }
        return $provider;
    }
}
