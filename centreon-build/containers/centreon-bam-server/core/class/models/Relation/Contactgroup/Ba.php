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

class Centreon_Object_Relation_Contact_Group_Ba extends Centreon_Object_Relation
{
    protected $relationTable = "mod_bam_cg_relation";
    protected $firstKey = "id_cg";
    protected $secondKey = "id_ba";

    /**
     * Constructor
     *
     * @return void
     */
    public function __construct()
    {
        Centreon_Object_Relation::__construct();
        $this->firstObject = new Centreon_Object_Contact_Group();
        $this->secondObject = new Ba();
    }
}
