<?php
/*
 * Copyright 2005-2015 Centreon
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

require_once realpath(dirname(__FILE__) . '/../common.php');

use \CentreonExport\Factory;

class CentreonExportTags extends CentreonWebService
{
    /**
     * 
     */
    public function __construct()
    {
        $this->pearDB = new \CentreonDB();
        $this->ExportFactory = new Factory();
        parent::__construct();
    }
    
    /**
     * 
     * @return array
     */
    public function getList()
    {
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

        $queryTags = "SELECT SQL_CALC_FOUND_ROWS DISTINCT tags_id, tags_name "
            . "FROM `mod_export_tags` LEFT JOIN `mod_export_tags_relation` ON tags_id = tag_id "
            . "WHERE tags_name LIKE '%$q%' "
            . " AND plugin_id = '$id'"
            . "ORDER BY tags_name "
            . $range;
                
        $DBRESULT = $this->pearDB->query($queryTags);

        $total = $this->pearDB->numberRows();
        
        $tagsList = array();
        while ($data = $DBRESULT->fetchRow()) {
            $tagsList[] = array(
                'id' => $data['tags_id'],
                'text' => $data['tags_name']
            );
        }
        
        return $tagsList;
    }
    
    
     /**
     * 
     * @return array
     */
    public function getAll()
    {
        // Check for select2 'q' argument
        if (false === isset($this->arguments['q'])) {
            $q = '';
        } else {
            $q = $this->arguments['q'];
        }

        if (isset($this->arguments['page_limit']) && isset($this->arguments['page'])) {
            $limit = ($this->arguments['page'] - 1) * $this->arguments['page_limit'];
            $range = 'LIMIT ' . $limit . ',' . $this->arguments['page_limit'];
        } else {
            $range = '';
        }

        $queryTags = "SELECT SQL_CALC_FOUND_ROWS DISTINCT tags_id, tags_name "
            . "FROM `mod_export_tags` "
            . "WHERE tags_name LIKE '%$q%' "
            . "ORDER BY tags_name "
            . $range;
        
        $DBRESULT = $this->pearDB->query($queryTags);

        $total = $this->pearDB->numberRows();
        
        $tagsList = array();
        while ($data = $DBRESULT->fetchRow()) {
            $tagsList[] = array(
                'id' => $data['tags_id'],
                'text' => $data['tags_name']
            );
        }
        
        return array(
            'items' => $tagsList,
            'total' => $total
        );
    }
}
