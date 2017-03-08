<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

namespace CentreonPollerDisplayCentral\ConfigGenerate\Bam;

use \CentreonPollerDisplayCentral\ConfigGenerate\Object;

/**
 * Description of BamBaseObject
 *
 * @author lionel
 */
class BamBaseObject extends Object
{
    /**
     *
     * @var array 
     */
    protected $comparisonKeys = array();
    
    /**
     * 
     * @param type $clauseObject
     * @return type
     */
    public function getList($clauseObject = null, $columnFilter = array())
    {        
        $selectedColumns = '';
        
        if (count($columnFilter) > 0) {
            $selectedColumns .= implode(',', $columnFilter);
        } else {
            $selectedColumns .= implode(',', $this->columns);
        }

        
        $queryGetList = "SELECT " .
            $selectedColumns .
            " FROM $this->table";
        
        if (isset($clauseObject) && count($clauseObject) > 0) {
            $this->buildClause($queryGetList, $clauseObject);
        }
        
        //echo $queryGetList . "\n";
        $resGetList = $this->db->query($queryGetList);
        
        $objectList = array();
        
        while ($rowGetList = $resGetList->fetch(\PDO::FETCH_ASSOC)) {
            $objectList[] = $rowGetList;
        }
        
        return $objectList;
    }
    
    /**
     * 
     * @param string $query
     * @param array $clauseObject
     */
    private function buildClause(&$queryGetList, $clauseObject)
    {
        $initLoop = true;
            
        foreach ($this->comparisonKeys as $comparisonKey) {
            
            if (isset($clauseObject[$comparisonKey])) {

                if ($initLoop) {
                    $queryGetList .=  " WHERE ";
                    $initLoop = false;
                } else {
                    $queryGetList .= " AND ";
                }

                $queryGetList .= $comparisonKey . " IN (" . implode(',', $clauseObject[$comparisonKey]) . ")";
            }

        }
    }
}
