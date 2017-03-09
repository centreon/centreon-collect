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
namespace CentreonPluginPackManager\Installation\Object;

/**
 *
 * @author lionel
 */
interface ObjectManagerInterface
{
    public function __construct($dbManager);

    public function getObjectParams();
    
    public function setObjectParams($newObjectParams);

    public function prepareInstall();

    public function prepareUpdate();

    public function prepareUninstall();

    public function launchInstall();

    public function launchUpdate();

    public function launchUninstall();
}
