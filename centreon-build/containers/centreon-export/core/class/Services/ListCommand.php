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

namespace CentreonExport\Services;

use \CentreonExport\DBManager;

class ListCommand
{
    /**
     *
     * @var type 
     */
    public $db;

    /**
     * 
     */
    public function __construct()
    {
        $this->db = new DBManager();
    }
 
    /**
     * This method returns the list of command
     * 
     * @param array $aIdCommand
     * @return array
     */
    public function getList($aIdCommand = array())
    {
        $listCommand = array();
        if (count($aIdCommand) == 0) {
            return $listCommand;
        }
        
        $sCommandId = implode(",", $aIdCommand);
        $queryList = "SELECT command_id, command_name, command_line FROM command  WHERE command_id in(".$sCommandId.")  ORDER BY command_name";
        $res = $this->db->db->query($queryList);
        
        while ($row = $res->fetch(\PDO::FETCH_ASSOC)) {
            $listCommand[$row['command_id']] = $row;
        }
        return $listCommand;
    }
}
