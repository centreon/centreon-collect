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

namespace CentreonPollerDisplayCentral\ConfigGenerate\Centreon;

use CentreonPollerDisplayCentral\ConfigGenerate\Object;

class Contact extends Object
{
    /**
     * @var table
     */
    protected $table = 'contact';

    /**
     * @var array
     * columns wanted
     */
    protected $columns = array(
        '*'
    );

    public function getList($clauseHostObject = null, $clauseServiceObject = null)
    {
        $listContact = array();
        $list = array();

        $hosts = $clauseHostObject;
        $services = $clauseServiceObject;

        $hErrors = array_filter($hosts);
        $sErrors = array_filter($services);

        if (empty($hErrors) && empty($sErrors)) {
            return '';
        }


        if (!empty($hErrors)) {
            foreach ($hosts as $host) {
                array_push($listContact, $host['contact_id']);
            }
        }

        if (!empty($sErrors)) {
            foreach ($services as $service) {
                array_push($listContact, $service['contact_id']);
            }
        }

        $listContactUnique = array_unique($listContact);
        $clauseQuery = ' WHERE contact_id IN (';
        $clauseQuery .= implode(',', $listContactUnique);
        $clauseQuery .= ')';

        $query = 'SELECT ' . implode(',', $this->columns) . ' '
            . 'FROM ' . $this->table . $clauseQuery;

        $result = $this->db->query($query);
        while ($row = $result->fetch(\PDO::FETCH_ASSOC)) {
            $list[] = $row;
        }

        return $list;
    }
}
