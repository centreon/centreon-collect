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

require_once "Centreon/Object/Relation/Relation.php";

class Centreon_Object_Relation_Ba_Group_Ba extends Centreon_Object_Relation
{
    protected $relationTable = "mod_bam_bagroup_ba_relation";
    protected $firstKey = "id_ba_group";
    protected $secondKey = "id_ba";

    /**
     * Constructor
     *
     * @return void
     */
    public function __construct()
    {
        Centreon_Object_Relation::__construct();
        $this->firstObject = new Ba_Group();
        $this->secondObject = new Ba();
    }
}
