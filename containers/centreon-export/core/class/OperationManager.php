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

namespace CentreonExport;

use \CentreonExport\DBManager;

require_once realpath(dirname(__FILE__) . "/../../../../../lib/Slug.class.php");

class OperationManager
{
    protected $db;
    const HTPL = 0;
    const STPL = 1;
    const CMD = 2;
    const STPLf = 3;
    public $plugin_id = 0;

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
        $this->plugin_id = $ppId;
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
        $this->plugin_id = $pluginPackId;
        /**
         * Get service_id and command_id to check if it's in use by multiple pp
         */
        $this->removeNotInUse($pluginPackId, self::HTPL);
        $this->removeNotInUse($pluginPackId, self::STPLf);
        $this->removeNotInUse($pluginPackId, self::CMD);

        $this->db->query("DELETE
            FROM mod_export_pluginpack
            WHERE plugin_id = {$this->db->escape($pluginPackId)}");

    }

    /**
     * Install plugin pack
     *
     * @param string $name
     * @param string $version
     * @return int | plugin pack id
     */
    public function install($name, $version, $release, $status, $status_message, $lastupdate)
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
            throw new \Exception('Invalid status for PP in XML file: "' . $status . '"');
        }

        $res = $this->db->query("SELECT plugin_id FROM mod_export_pluginpack WHERE plugin_name = '{$this->db->escape($name)}'");
        $sSlug = (string) new \Slug($name, array('max_length' => 255));
        $sDiplayName = substr($name, 0, 15);

        if ($res->numRows()) {
            $row = $res->fetchRow();
            $pluginPackId = $row['plugin_id'];
            $this->db->query("UPDATE mod_export_pluginpack
            SET
            plugin_version = '$version',
            plugin_slug = '$sSlug',
            plugin_display_name = '$sDiplayName',
            plugin_status = '$status',
            plugin_status_message = '$status_message',
            plugin_last_update = '$lastupdate'    
            WHERE plugin_id = {$pluginPackId}");
        } else {
            $query = "INSERT INTO mod_export_pluginpack (plugin_name, plugin_slug, plugin_display_name, plugin_version, plugin_status, plugin_status_message, plugin_last_update)
                VALUES ('$name', '$sSlug', '$sDiplayName', '$version', '$status', '$status_message', '$lastupdate')";

            $DBRESULT = $this->db->query($query);
            
            $res = $this->db->query("SELECT MAX(plugin_id) as last_id
                FROM mod_export_pluginpack
                WHERE plugin_name = '$name'");
            $row = $res->fetchRow();
            $pluginPackId = $row['last_id'];
        }
       
        $this->plugin_id = $pluginPackId;
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
                $table = "mod_export_hostTpl";
                $field = "host_id";
                break;
            case self::STPL:
                $table = "mod_export_serviceTpl";
                $field = "service_id";
                break;
            case self::CMD:
                $table = "mod_export_command";
                $field = "command_id";
                break;
            default:
                throw new \Exception('Unknown object type');
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
                $this->db->query("INSERT INTO {$table} (plugin_id,
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
    public function getRelations($pluginPackId, $objectType) {
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_export_hostTpl";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_export_serviceTpl";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                break;
            case self::CMD:
                $table = "mod_export_command";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                break;
            default:
                throw new \Exception('Unknown object type');
        }
        $res = $this->db->query("SELECT pp.plugin_id, op.{$field}, op.{$labelfield}
                FROM {$table} pp, {$objecttable} op
                WHERE op.{$field} = pp.{$field}
                AND pp.plugin_id = {$this->db->escape($pluginPackId)}
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
    public function objectInUse($objectType, $objectId) {
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
                    AND NOT EXISTS (SELECT pp.service_id FROM mod_export_serviceTpl pp WHERE pp.service_id = s.service_id AND pp.plugin_id = ".$this->plugin_id.")";
                break;
            case self::CMD:
                // Service/Hosts count that are using this command
                $sql = "SELECT COUNT(*) as nb
                    FROM ( SELECT service_description FROM service WHERE command_command_id = {$this->db->escape($objectId)} UNION SELECT host_name FROM host WHERE command_command_id = {$this->db->escape($objectId)}) as tab
                    WHERE NOT EXISTS (SELECT pp.service_id FROM mod_export_serviceTpl pp, mod_pluginpack WHERE pp.plugin_id = mod_pluginpack.plugin_id)";
                break;
            default:
                throw new \Exception('Unknown object type: '.$objectType);
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
    public function removeNotInUse($pluginPackId, $objectType) {
        $objectRelations = $this->getRelations($pluginPackId, $objectType);
        switch ($objectType) {
            case self::HTPL:
                $table = "mod_export_hostTpl";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_export_serviceTpl";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                break;
            case self::CMD:
                $table = "mod_export_command";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                break;
            default:
                throw new \Exception('Unknown object type');
        }
        $extra = "";
        if ($objectType == self::STPL) {
            $extra = " AND op.service_locked = 1 AND mod_export_serviceTpl.status = '1' ";
        }
        if ($objectType == self::HTPL) {
            $extra = " AND mod_export_hostTpl.status = '1' ";
        }
        foreach($objectRelations as $objectId => $id){
            if ($this->objectInUse($objectType, $objectId)) {
                throw new \Exception($objecttable.' "'.$id[$labelfield].'" is in use, cannot remove it');
            }
            $sql = "SELECT COUNT({$field}) as nb FROM {$table} pp WHERE pp.{$field} = {$this->db->escape($objectId)}";
            $res = $this->db->query($sql);
            $row = $res->fetchRow();
            // Delete object if only 1 PP is the owner
            if ($row['nb'] == 1) {
                $this->db->query("DELETE FROM {$objecttable}
                    WHERE {$field} IN (SELECT {$field}
                    FROM {$table}
                    WHERE plugin_id = {$this->db->escape($pluginPackId)}
                    AND {$field} = {$this->db->escape($objectId)})"
                );
            }
            // Delete link between PP and object in all case
            $this->db->query("DELETE FROM {$table}
                WHERE {$field} = {$this->db->escape($objectId)} AND plugin_id = {$this->db->escape($pluginPackId)}"
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
                $table = "mod_export_hostTpl";
                $field = "host_id";
                $labelfield = "host_name";
                $objecttable = "host";
                $sections = "hosts";
                break;
            case self::STPL:
            case self::STPLf:
                $table = "mod_export_serviceTpl";
                $field = "service_id";
                $labelfield = "service_description";
                $objecttable = "service";
                $sections = "templates";
                break;
            case self::CMD:
                $table = "mod_export_command";
                $field = "command_id";
                $labelfield = "command_name";
                $objecttable = "command";
                $sections = "commands";
                break;
            default:
                throw new \Exception('Unknown object type');
        }
        $extra = "";
        if ($objectType == self::STPL) {
            $extra = " AND op.service_locked = 1 AND mod_export_serviceTpl.status = '1' ";
        }
        if ($objectType == self::HTPL) {
            $extra = " AND mod_export_hostTpl.status = '1' ";
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
                        throw new \Exception($objecttable.' \''.$params[$labelfield].'\' already exists outside of PP');
                    }
                }
            }
        }
    }
}
