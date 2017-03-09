<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2015 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */

/**
 * This method is a view helper for debugging PHP arrays
 * 
 * @param array $aDatas
 */
function print_rf(&$aDatas)
{
    echo "<pre>";
    print_r($aDatas);
}

/**
 * This method cleans the variables before sending to other methods 
 * 
 * @param string $element
 * @param string $type
 * @return string
 */
function checkFilter($element, $type = 'text')
{
    $sReturn = '';
    if (is_string($element)) {
        switch ($type) {
            case 'date':
                $sReturn = convertDate($element);
                break;
            default:
                $sReturn = trim($element);
                break;
        }
    }
    return $sReturn;
}

/**
 * This convert date javascipt to the format sql
 * 
 * @param string $sDate date
 * 
 * Exemple:
 * 01/05/2016 => '2016-01-05'
 */
function convertDate($sDate)
{
    if (empty($sDate)) {
        return;
    }
    $aArray = explode("/", $sDate);
    if (count($aArray) != 3 || strlen($aArray[2]) != 4) {
        return;
    }
    
    return $aArray[2] . '-' .  str_pad($aArray[1], 2, '0', STR_PAD_LEFT) . '-' .  str_pad($aArray[0], 2, '0', STR_PAD_LEFT). " 00:00:00";
}


function cleanData($element)
{
    if (is_string($element)) {
        $element = trim($element);
    }
    return $element;
}

function testPluginExistence($name = null)
{
    global $pearDB, $form;

    $id = null;
    if (isset($form)) {
        $id = $form->getSubmitValue('plugin_id');
    }
    $DBRESULT = $pearDB->query("SELECT plugin_name, plugin_id FROM `mod_export_pluginpack` WHERE plugin_name = '" . CentreonDB::escape($name) . "'");
    $host = $DBRESULT->fetchRow();

    /*
     * Modif case
     */

    if ($DBRESULT->numRows() >= 1 && $host["plugin_id"] == $id) {
        return true;
    }
    /*
     * Duplicate entry
     */
    elseif ($DBRESULT->numRows() >= 1 && $host["plugin_id"] != $id) {
        return false;
    } else {
        return true;
    }
}

function in_array_r($needle, $haystack, $strict = false) {
    foreach ($haystack as $item) {
        if (($strict ? $item === $needle : $item == $needle) || (is_array($item) && in_array_r($needle, $item, $strict))) {
            return true;
        }
    }

    return false;
}
