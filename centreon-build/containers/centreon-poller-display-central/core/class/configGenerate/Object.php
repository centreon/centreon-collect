<?php
/*
 * Copyright 2005-2017 Centreon
 * Centreon is developped by : Julien Mathis and Romain Le Merlus under
 * GPL Licence 2.0.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation ; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking this program statically or dynamically with other modules is making a
 * combined work based on this program. Thus, the terms and conditions of the GNU
 * General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this program give Centreon
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of Centreon choice, provided that
 * Centreon also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonPollerDisplayCentral\ConfigGenerate;

/**
 * User: kduret
 * Date: 23/02/2017
 * Time: 09:19
 */
abstract class Object
{
    /**
     *
     * @var \CentreonDB 
     */
    protected $db = null;
    
    /**
     *
     * @var int 
     */
    protected $pollerId = null;
    
    /**
     *
     * @var string 
     */
    protected $table = null;
    
    /**
     *
     * @var array 
     */
    protected $columns = null;
    
    /**
     *
     * @var string 
     */
    protected $primaryKey = null;

    /**
     * 
     * @param \CentreonDB $db
     * @param int $pollerId
     * @param array $filteredObjects
     */
    public function __construct($db, $pollerId)
    {
        $this->db = $db;
        $this->pollerId = $pollerId;
    }

    /**
     * 
     * @param array $object
     * @return string
     */
    public function generateSql($object = array(), $withCleaning = true)
    {
        if (is_null($this->table) || is_null($this->columns)) {
            return null;
        }

        if ($withCleaning) {
            $deleteQuery = $this->generateDeleteQuery();
            $truncateQuery = $this->generateTruncateQuery();
        }

        $insertQuery = $this->generateInsertQuery($object);

        $finalQuery = $deleteQuery . "\n" . $truncateQuery . "\n" . $insertQuery;

        return $finalQuery;
    }

    /**
     * 
     * @return string
     */
    protected function generateTruncateQuery()
    {
        $query = 'TRUNCATE ' . $this->table . ';';
        return $query;
    }

    /**
     * 
     * @return string
     */
    protected function generateDeleteQuery()
    {
        $query = 'DELETE FROM ' . $this->table . ';';
        return $query;
    }

    /**
     * 
     * @param type $objects
     * @return string
     */
    protected function generateInsertQuery($objects)
    {
        $errors = array_filter($objects);
        if (empty($errors)) {
            return '';
        }
        
        if (implode(',', $this->columns) == '*') {
            $this->columns = array_keys($objects[0]);
        }

        $insertQuery = 'INSERT INTO `' . $this->table . '` '
            . '(`' . implode('`,`', $this->columns) . '`) '
            . 'VALUES ';

        $first = true;
        foreach ($objects as $object) {
            if (!$first) {
                $insertQuery .= ',';
            }
            $insertQuery .= '(';
            foreach ($object as $value) {
                if (is_null($value)) {
                    $insertQuery .= 'NULL,';
                } else {
                    $insertQuery .= '\'' . $value . '\',';
                }
            }
            $insertQuery = rtrim($insertQuery, ',');
            $insertQuery .= ')';
            $first = false;
        }
        $insertQuery .= ';';

        return $insertQuery;
    }

    /**
     * 
     * @param type $clauseObject
     * @return array
     */
    public function getList($clauseObject = null)
    {
        $list = array();

        $query = 'SELECT ' . implode(',', $this->columns) . ' '
            . 'FROM ' . $this->table . ' ';
        
        $result = $this->db->query($query);

        while ($row = $result->fetch(\PDO::FETCH_ASSOC)) {
            $list[] = $row;
        }

        return $list;
    }
}
