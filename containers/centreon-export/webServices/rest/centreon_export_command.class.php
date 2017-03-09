<?php
/*
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

require_once realpath(dirname(__FILE__) . '/../common.php');

use \CentreonExport\Factory;

class CentreonExportCommand extends CentreonWebService
{
    /**
     *
     * @var type 
     */
    protected $pearDB;
    
    /**
     * Constructor
     */
    public function __construct()
    {
        $this->pearDB = new CentreonDB();
        $this->ExportFactory = new Factory();
        parent::__construct();
    }

    /**
     * Get default report values
     * 
     * @param array $args
     */
    public function getList()
    {
        global $centreon;
        // Check for select2 'q' argument
        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }

        
        if (false === isset($this->arguments['id'])) {
            $id = '';
        } else {
            $id = $this->arguments['id'];
        }
        
        if (isset($this->arguments['page_limit']) && isset($this->arguments['page'])) {
            $limit = ($this->arguments['page'] - 1) * $this->arguments['page_limit'];
            $range = 'LIMIT ' . $limit . ',' . $this->arguments['page_limit'];
        } else {
            $range = '';
        }
        
        $queryCommand = "SELECT SQL_CALC_FOUND_ROWS exp.command_id, command_name "
            . "FROM `mod_export_command` as exp JOIN `command` as cmd ON exp.command_id = cmd.command_id "
            . " WHERE command_name LIKE '%$q%' "
            . " AND plugin_id = '$id'"
            . "ORDER BY command_name "
            . $range;
        
        
        $DBRESULT = $this->pearDB->query($queryCommand);

        $total = $this->pearDB->numberRows();
        
        $commandList = array();
        while ($data = $DBRESULT->fetchRow()) {
            $commandList[] = array('id' => $data['command_id'], 'text' => $data['command_name']);
        }
        
        return $commandList;
    }
}
