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

require_once "Centreon/Object/Object.php";

/**
 * Description of boolean kpi
 *
 * @author kduret <kduret@centreon.com>
 */
class BooleanRule extends Centreon_Object {
    protected $table = "mod_bam_boolean";
    protected $primaryKey = "boolean_id";
    protected $uniqueLabelField = "name";

}
