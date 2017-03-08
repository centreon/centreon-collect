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

function versionCentreon($pearDB)
{
    if (is_null($pearDB)) {
        return;
    }
    $query = 'SELECT `value` FROM `informations` WHERE `key` = "version"';
    $res = $pearDB->query($query);
    if (PEAR::isError($res)) {
        return null;
    }
    $row = $res->fetchRow();
    return $row['value'];
}

function getCategories($pearDB)
{
    $res = $pearDB->query("SELECT slug, name FROM mod_ppm_discovery_category ORDER BY name");

    if (PEAR::isError($res)) {
        $res->getMessage() . "\n" . $res->getUserInfo();
    }

    $list = array('' => '');
    while ($row = $res->fetchRow()) {
        $list[$row['slug']] = $row['name'];
    }

    // Sort categories by name
    asort($list);

    return $list;
}
