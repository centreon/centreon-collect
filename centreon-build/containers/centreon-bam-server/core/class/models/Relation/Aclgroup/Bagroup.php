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

class Centreon_Object_Relation_Acl_Group_Ba_Group extends Centreon_Object_Relation
{
    protected $relationTable = "mod_bam_acl";
    protected $firstKey = "acl_group_id";
    protected $secondKey = "ba_group_id";

    /**
     * Constructor
     *
     * @return void
     */
    public function __construct()
    {
        Centreon_Object_Relation::__construct();
        $this->firstObject = new Ba();
        $this->secondObject = new Centreon_Object_Acl_Group();
    }
}
