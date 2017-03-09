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

namespace CentreonPollerDisplayCentral;

/**
 * Class to manage poller display.
 * User: kduret
 * Date: 23/02/2017
 * Time: 09:19
 */
class PollerDisplay
{
    protected $db;

    /**
     * PollerDisplay constructor.
     * @param $db
     */
    public function __construct($db)
    {
        $this->db = $db;
    }

    /**
     * Delete values in database
     *
     * @param int|null $id
     */
    public function delete($id = null)
    {
        $query = 'DELETE FROM mod_poller_display_server_relations ';

        if (!is_null($id)) {
            $query .= 'WHERE nagios_server_id = ' . $this->db->escape($id) . ' ';
        }

        $this->db->query($query);
    }

    /**
     * Insert values in database
     *
     * @param array $ids
     * @return null
     */
    public function insert($ids = array())
    {
        if (!count($ids)) {
            return null;
        }

        $query = 'INSERT INTO mod_poller_display_server_relations (nagios_server_id) '
            . 'VALUES (' . implode('),(', $ids) . ') ';
        $this->db->query($query);
    }

    /**
     * Insert values from configuration form
     *
     * @param array $postValues
     */
    public function insertFromForm($postValues = array())
    {
        $pollerIds = isset($postValues['poller_display']) ? array_values($postValues['poller_display']) : array();

        $this->delete();
        $this->insert($pollerIds);
    }

    /**
     * Get list of poller which are poller display
     *
     * @return array of pollers
     */
    public function getList()
    {
        $query = 'SELECT ns.id, ns.name '
            . 'FROM nagios_server ns '
            . 'INNER JOIN mod_poller_display_server_relations sr '
            . 'ON sr.nagios_server_id = ns.id ';
        $result = $this->db->query($query);

        $pollers = array();
        while ($row = $result->fetchRow()) {
            $pollers[$row['name']] = $row['id'];
        }

        return $pollers;
    }
}
