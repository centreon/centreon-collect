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

require_once dirname(__FILE__) . '/License.class.php';

class CentreonPluginPack
{
    protected $db;
    const HTPL = 0;
    const STPL = 1;
    const CMD = 2;
    const STPLf = 3;
    public $pp_id = 0;

    /**
     *
     * @var string Applicate new graphique chart
     */
    public static $sVersionCentreonNewMgt = '2.6.99';

    /**
     * Constructor
     *
     * @param CentreonDB $db
     */
    public function __construct($db, $ppId = 0)
    {
        $this->db = $db;
        $this->pp_id=$ppId;
    }

    /**
     * Returns list of plugin packs
     *
     * @return array
     */
    public function getList($num = '', $limit = '', $iTotal = '')
    {

        if (!empty($limit)) {
            if ($limit == $iTotal) {
                $num = 0;
            }
            $sQuery = "SELECT pp_id, pp_name, pp_version, pp_release, case when pp_status in (null, '') then 'unknown' else pp_status end as pp_status, pp_status_message
                FROM mod_pluginpack ORDER BY pp_name LIMIT ".$num * $limit.", ".$limit;
        } else {
            $sQuery = "SELECT pp_id, pp_name, pp_version, pp_release, case when pp_status in (null, '') then 'unknown' else pp_status end as pp_status, pp_status_message
                FROM mod_pluginpack ORDER BY pp_name";
        }

        $res = $this->db->query($sQuery);
        $arr = array();
        while ($row = $res->fetchRow()) {
            $arr[$row['pp_id']] = $row;
        }
        return $arr;
    }

    /**
     * Returns number of plugin packs
     *
     * @return integer
     */
    public function getCount()
    {
        $sQuery = "SELECT count(*) FROM mod_pluginpack";
        $DBRESULT = $this->db->query($sQuery);

        $tmp = $DBRESULT->fetchRow();
        $rows = $tmp["count(*)"];
        return $rows;
    }

    /**
     * Uninstall plugin pack
     * Also removes relations
     *
     * @param int $pluginPackId
     * @return void
     */
    public function uninstall($pluginPackId)
    {
        $this->pp_id = $pluginPackId;
        /**
         * Get service_id and command_id to check if it's in use by multiple pp
         */
        $this->removeNotInUse($pluginPackId, self::HTPL);
        $this->removeNotInUse($pluginPackId, self::STPLf);
        $this->removeNotInUse($pluginPackId, self::CMD);

        $this->db->query("DELETE
            FROM mod_pluginpack
            WHERE pp_id = {$this->db->escape($pluginPackId)}");

    }

    /**
     * Install plugin pack
     *
     * @param string $name
     * @param string $version
     * @return int | plugin pack id
     */
    public function install($name, $version, $release, $status, $status_message)
    {
        $name = $this->db->escape($name);
        $release = $this->db->escape($release);
        $status = $this->db->escape($status);
        $status == NULL ? '' : $status;
        $status_message = $this->db->escape($status_message);
        $status_message == NULL ? '' : $status_message;

        $valid_statuses = array('stable', 'testing', 'dev', 'experimental', 'deprecated');

        // $status may be empty (for compat' reasons), but must belong to a limited set of values
        if (isset($status) && $status !== '' && ! in_array($status, $valid_statuses)) {
            throw new Exception('Invalid status for PP in XML file: "' . $status . '"');
        }

        $res = $this->db->query("SELECT pp_id
            FROM mod_pluginpack
            WHERE pp_name = '{$this->db->escape($name)}'");
        if ($res->numRows()) {
            $row = $res->fetchRow();
            $pluginPackId = $row['pp_id'];
        } else {
            $this->db->query("INSERT INTO mod_pluginpack (pp_name, pp_release, pp_status, pp_status_message)
                VALUES ('$name','$release','$status','$status_message')");
            $res = $this->db->query("SELECT MAX(pp_id) as last_id
                FROM mod_pluginpack
                WHERE pp_name = '$name'");
            $row = $res->fetchRow();
            $pluginPackId = $row['last_id'];
        }
        $this->db->query("UPDATE mod_pluginpack
            SET
            pp_version = '$version',
            pp_release = '$release',
            pp_status = '$status',
            pp_status_message = '$status_message'
            WHERE pp_id = {$pluginPackId}");
        $this->pp_id = $pluginPackId;
        return $pluginPackId;
    }

    /**
     * Set relations for either host templates, service templates or commands
     *
     * @param int $pluginPackId
     * @param int $objectType
     * @param array $objectIds
     * @throws Exception
     */
    public function setRelations($pluginPackId, $objectType, $objectIds)
    {
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_pp_host_templates";
                $field = "host_id";
                break;
            case self::STPL:
                $table = "mod_pp_service_templates";
                $field = "service_id";
                break;
            case self::CMD:
                $table = "mod_pp_commands";
                $field = "command_id";
                break;
            default:
                throw new Exception('Unknown object type');
        }
        if (is_array($objectIds)) {
            $str = "";
            foreach ($objectIds as $id) {
                if ($str != "") {
                    $str .= ",";
                }
                $str .= "({$pluginPackId},{$id})";
            }
            if ($str != "") {
                $this->db->query("INSERT INTO {$table} (pp_id,
                    {$field}) VALUES {$str}");
            }
        }
    }

    /**
     * Get prefix from array of strings
     *
     * @param array $arrpty
     * @return string
     */
    public function getPrefix($arr = array())
    {
        if (count($arr) <= 2) {
            return "";
        }
        $prefix = null;
        foreach ($arr as $s) {
            if (is_null($prefix)) {
                $prefix = $s;
            } else {
                while(!empty($prefix) && substr($s, 0, strlen($prefix)) != $prefix) {
                    $prefix = substr($prefix, 0, -1);
                }
            }
        }
        return $prefix;
    }

    /**
     * Return the number of relations
     *
     * @param int $pluginPackId
     * @param int $objectType
     * @return array
     */
    public function getRelations($pluginPackId, $objectType)
    {
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_pp_host_templates";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_pp_service_templates";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                break;
            case self::CMD:
                $table = "mod_pp_commands";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                break;
            default:
                throw new Exception('Unknown object type');
        }
        $extra = "";
        /*if ($objectType == self::STPL) {
            $extra = " AND op.service_locked = 1 ";
        }*/
        $res = $this->db->query("SELECT pp.pp_id, op.{$field}, op.{$labelfield}
                FROM {$table} pp, {$objecttable} op
                WHERE op.{$field} = pp.{$field}
                AND pp.pp_id = {$this->db->escape($pluginPackId)}
                {$extra}
                ORDER BY {$labelfield}");
        $arr = array();
        while ($row = $res->fetchRow()) {
            $arr[$row[$field]] = $row;
        }
        return $arr;
    }

    /**
     * Return number of objects that are currently using $objectId
     *
     * @param int $objectType
     * @param int $objectId
     * @return int
     */
    public function objectInUse($objectType, $objectId)
    {
        switch ($objectType) {
            case self::HTPL:
                // Host count that are using this host templates
                $sql = "SELECT COUNT(host_host_id) as nb
                    FROM host_template_relation
                    WHERE host_tpl_id = {$this->db->escape($objectId)}";
                break;
            case self::STPL:
            case self::STPLf:
                // Service count that are using this service template and that are not *-custom services provided by the pack
                $sql = "SELECT COUNT(service_id) as nb
                    FROM service s
                    WHERE service_template_model_stm_id = {$this->db->escape($objectId)}
                    AND NOT EXISTS (SELECT pp.service_id FROM mod_pp_service_templates pp WHERE pp.service_id = s.service_id AND pp.pp_id = ".$this->pp_id.")";
                break;
            case self::CMD:
                // Service/Hosts count that are using this command
                $sql = "SELECT COUNT(*) as nb
                    FROM ( SELECT service_description FROM service WHERE command_command_id = {$this->db->escape($objectId)} UNION SELECT host_name FROM host WHERE command_command_id = {$this->db->escape($objectId)}) as tab
                    WHERE NOT EXISTS (SELECT pp.service_id FROM mod_pp_service_templates pp, mod_pluginpack WHERE pp.pp_id = mod_pluginpack.pp_id)";
                break;
            default:
                throw new Exception('Unknown object type: '.$objectType);
        }
        $res = $this->db->query($sql);
        $row = $res->fetchRow();
        return $row['nb'];
    }

    /**
     * Delete entries if not in use
     *
     * @param int $pluginPackId
     * @param int $objectType
     * @return array
     */
    public function removeNotInUse($pluginPackId, $objectType)
    {
        $objectRelations = $this->getRelations($pluginPackId, $objectType);
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_pp_host_templates";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_pp_service_templates";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                break;
            case self::CMD:
                $table = "mod_pp_commands";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                break;
            default:
                throw new Exception('Unknown object type');
        }
        $extra = "";
        if ($objectType == self::STPL) {
            $extra = " AND op.service_locked = 1 ";
        }

        foreach($objectRelations as $objectId => $id){
            if ($this->objectInUse($objectType, $objectId)) {
                throw new Exception($objecttable.' "'.$id[$labelfield].'" is in use, cannot remove it');
            }
            $sql = "SELECT COUNT({$field}) as nb FROM {$table} pp WHERE pp.{$field} = {$this->db->escape($objectId)}";
            $res = $this->db->query($sql);
            $row = $res->fetchRow();
            // Delete object if only 1 PP is the owner
            if ($row['nb'] == 1) {
                $this->db->query("DELETE FROM {$objecttable}
                    WHERE {$field} IN (SELECT {$field}
                    FROM {$table}
                    WHERE pp_id = {$this->db->escape($pluginPackId)}
                    AND {$field} = {$this->db->escape($objectId)})"
                );
            }
            // Delete link between PP and object in all case
            $this->db->query("DELETE FROM {$table}
                WHERE {$field} = {$this->db->escape($objectId)} AND pp_id = {$this->db->escape($pluginPackId)}"
            );
        }
    }

    /**
     * check for already installed commands, services or hosts outside PP
     *
     * @param int $pluginPackId
     * @param int $objectType
     * @return array
     */
    public function checkOutsidePP($XMLdata, $objectType)
    {
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_pp_host_templates";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                $sections = "hosts";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_pp_service_templates";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                $sections = "templates";
                break;
            case self::CMD:
                $table = "mod_pp_commands";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                $sections = "commands";
                break;
            default:
                throw new Exception('Unknown object type');
        }
        $extra = "";
        if ($objectType == self::STPL) {
            $extra = " AND op.service_locked = 1 ";
        }

        foreach ($XMLdata->$sections as $section) {
            foreach ($section as $key => $value) {
                $params = array();
                foreach ($value->children() as $key => $str) {
                    $params[$key] = $str;
                }
                // get id of existing object
                $sql = "SELECT {$field} FROM {$objecttable} WHERE {$labelfield} LIKE '".$params[$labelfield]."'";
                $res = $this->db->query($sql);
                if ($res->numRows()) {
                    $data = $res->fetchRow();
                    // if object exists, get it's id into PP tables
                    $ret=$this->db->query("SELECT * FROM {$table} WHERE {$field} = ".$data[$field]);
                    if(! $ret->numRows()) {
                        // throw exception if id doesn't exists into PP tables
                        throw new Exception($objecttable.' \''.$params[$labelfield].'\' already exists outside of PP');
                    }
                }
            }
        }
    }
    
    /**
     * Return the correct connector for the module
     *
     * @param CentreonDB $database The database connection
     * @param Object $apiLib API lib
     */
    public static function factory($database = null, $apiLib = null)
    {
        if (is_null($database)) {
            $database = new CentreonDB();
        }
        if (CentreonPluginPackManager\License::isValid()) {
            require_once 'centreonPluginPack/EMS.php';
            require_once 'EmsApi.class.php';
            if (is_null($apiLib)) {
                $pluginPackManagerFactory = new \PluginPackManagerFactory($database);
                $apiLib = $pluginPackManagerFactory->newEmsApi();
            }

            return new CentreonPluginPack_EMS($database, $apiLib);
        } else {
            require_once 'centreonPluginPack/IMP.php';
            require_once _CENTREON_PATH_ . '/www/class/centreonRestHttp.class.php';
            require_once 'System.class.php';
            require_once 'Crypto.class.php';
            require_once 'Fingerprint.class.php';
            require_once 'ImpApi.class.php';
            if (is_null($apiLib)) {
                $pluginPackManagerFactory = new \PluginPackManagerFactory($database);
                $apiLib = $pluginPackManagerFactory->newImpApi(CENTREON_IMP_API_URL);
            }

            return new CentreonPluginPack_IMP($database, $apiLib);
        }
    }
}