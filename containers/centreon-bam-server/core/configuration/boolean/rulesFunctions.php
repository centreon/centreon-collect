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

function testExpValidity() {
    global $bool, $form;
    
    try {
        $expr = $form->getSubmitValue('expression');
        $bool->evalExpr($expr, array(), false);
        return true;
    } catch (Exception $e) {
        return false;
    }
}

function testBoolExistence() {
    global $pearDB;
    global $form;

    if (!isset($form)) {
        return false;
    }

    $id = $form->getSubmitValue('boolean_id');    
    $name = $form->getSubmitValue('name');

    if (!$id) {
        $id = 0;
    }
    $sql = "SELECT boolean_id FROM mod_bam_boolean 
            WHERE name = '".$pearDB->escape($name)."'
            AND boolean_id != ".$pearDB->escape($id);
    $res = $pearDB->query($sql);
    if ($res->numRows()) {
        return false;
    }
    return true;
}
