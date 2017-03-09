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

class Centreon_Object_Relation_Instance_Ba extends Centreon_Object_Relation
{
    protected $relationTable = "mod_bam_poller_relations";
    protected $firstKey = "poller_id";
    protected $secondKey = "ba_id";

    /**
     * Constructor
     *
     * @return void
     */
    public function __construct()
    {
        Centreon_Object_Relation::__construct();
        $this->firstObject = new Centreon_Object_Instance();
        $this->secondObject = new Ba();
    }
}
