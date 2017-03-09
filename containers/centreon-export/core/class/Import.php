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
use \CentreonExport\OperationManager;
use \CentreonExport\LogMessage;

/**
 * Centreon Import Class
 */
class Import
{

    protected $db;
    protected $sFileXml;
    protected $translationId;
    protected $ppObj;
    protected $pluginPackId;

    /**
     * Constructor
     *
     * @param CentreonDB $db
     */
    public function __construct($db, $sFileXml)
    {
        $this->db = $db;
        $this->sFileXml = $sFileXml;
        $this->translationId = array(
            "cmd" => array(),
            "host" => array(),
            "svc" => array(),
            "tp" => array(),
            "trap" => array(),
            "vendor" => array()
        );
        $this->ppObj = new OperationManager($this->db);
        $this->logs = new LogMessage();
    }

    /**
     * Init installl
     * 
     * @return void
     * @throws Exception
     */
    public function initInstall($name, $version, $release, $status, $status_message, $lastupdate)
    {

        $this->logMessage("Installing " . $name);
        /**
         * check for already installed commands, services or hosts outside PP
         */
        $tempXml = $this->getXmlObject();
        $this->ppObj->checkOutsidePP($tempXml, OperationManager::CMD);
        $this->ppObj->checkOutsidePP($tempXml, OperationManager::HTPL);
        $this->ppObj->checkOutsidePP($tempXml, OperationManager::STPLf);

        $this->pluginPackId = $this->ppObj->install($name, $version, $release, $status, $status_message, $lastupdate);
    }

    /**
     * Get File
     *
     * @param array $arg
     * @throws Exception
     * @return SimpleXMLElement
     */
    public function getXmlObject()
    {
        if (!isset($this->sFileXml)) {
            throw new \Exception('No XML file specified');
        }
        if (!is_readable($this->sFileXml)) {
            throw new \Exception('XML File is not readable');
        }
        $clean_str = file_get_contents($this->sFileXml);
        $xmlString = simplexml_load_string($clean_str);
        if ($xmlString === false) {
            throw new \Exception('Incorrect XML format in file');
        }
        return $xmlString;
    }

    /**
     * Insert host relation
     *
     * @param array $relationId
     * @param int $hostId
     * @return void
     */
    public function insertHostRelations($relationId, $hostId)
    {
        $str = "";
        $order = 1;
        foreach ($relationId as $tplId) {
            if ($str != "") {
                $str .= ", ";
            }
            $str .= "('" . $tplId . "', '" . $hostId . "', " . $order . ")";
            $order++;
        }
        if ($str != "") {
            $query = "INSERT INTO host_template_relation (`host_tpl_id`, `host_host_id`, `order`) VALUES $str";
            $this->db->query($query);
            $this->logMessage("Inserted host template relations");
        }
    }

    /**
     * Insert Service Relations
     *
     * @param array $svcTplId
     * @param int $hostId
     * @return void
     */
    public function insertServiceRelations($svcTplId, $hostId)
    {
        $str = "";
        foreach ($svcTplId as $tplId) {
            if ($str != "") {
                $str .= ", ";
            }
            $str .= "('" . $tplId . "', '" . $hostId . "')";
        }
        if ($str != "") {
            $query = "INSERT INTO host_service_relation (service_service_id, host_host_id) VALUES $str";
            $this->db->query($query);
            $this->logMessage("Inserted service relations");
        }
    }

    /**
     * Update service
     * Returns true if update is made, false otherwise
     *
     * @param int $serviceId
     * @param array $tmpSvc
     * @return void
     */
    public function updateService($serviceId, $tmpSvc, $params)
    {
        $templateName = "";

        $query = "UPDATE service SET ";
        $condition = " WHERE service_id = " . $this->db->escape($serviceId);
        $count = 0;
        foreach ($tmpSvc as $key => $val) {
            // Do not update for Custom
            if ($key == "macros" && !preg_match('/-Custom$/i', $params['service_description'])) {
                $this->updateMacros($val, $serviceId, "service");
            } elseif ($key == "traprelations" && !preg_match('/-Custom$/i', $params['service_description'])) {
                $this->updateServiceTrapRelations($val, $serviceId);
            } elseif ($key != "service_id") {
                $val = trim($val);
                if ($key == 'service_template_name') {
                    $templateName = $val;
                    continue;
                }
                if (($key == "command_command_id" || $key == "command_command_id2") && isset($this->translationId["cmd"][$val])) {
                    $val = $this->translationId["cmd"][$val];
                }
                if (($key == "timeperiod_tp_id" || $key == "timeperiod_tp_id2") && isset($this->translationId["tp"][$val])) {
                    $val = $this->translationId["tp"][$val];
                }
                if ($key == "service_template_model_stm_id") {
                    if (isset($this->translationId['svc'][$val])) {
                        $val = $this->translationId['svc'][$val];
                    } else {
                        // See comment for same code in insertService()
                        if ($tmpSvc['service_description'] != 'generic-active-service') {
                            if (isset($params['service_template_name']) && $params['service_template_name'] != '') {
                                if ($params['service_template_name'] == 'generic-active-service' || preg_match('/-custom$/i', $params['service_template_name'])) {
                                    $val = $this->getServiceTplIdByDescription($params['service_template_name']);
                                } else {
                                    throw new \Exception("Parent template of '" . $tmpSvc['service_description'] . "' is '" . $params['service_template_name'] . "', it must be a '*-custom'. Invalid template.xml.\n");
                                }
                            } else {
                                throw new \Exception("No parent template defined for template '" . $tmpSvc['service_description'] . "'. Invalid template.xml.\n");
                            }
                        }
                    }
                }

                // Update only service template relation and service_description (user can change service_alias)
                if ((preg_match('/-Custom$/i', $params['service_description']) && ($key == 'service_description' || $key == 'service_template_model_stm_id')) || (!preg_match('/-Custom$/i', $params['service_description']) &&
                    ($key == 'command_command_id' ||
                    $key == 'command_command_id2' ||
                    $key == 'service_active_checks_enabled' ||
                    $key == 'service_passive_checks_enabled'))) {

                    if ($val != "") {
                        $val = "'" . $this->db->escape($val) . "'";
                    } else {
                        $val = "NULL";
                    }

                    if ($key == 'service_description') {
                        $name = "'" . $this->db->escape($val) . "'";
                    }

                    if ($count) {
                        $query .= ", ";
                    }
                    $query .= $key . " = " . (($val == "") ? "NULL" : $val);
                    $count++;
                }
            }
        }

        if ($count) {
            $query .= $condition;
            $this->db->query($query);
        }

        // Update relation even if template has been created by an other plugin
        $ppServiceTemplates[] = $serviceId;
        $relations = $this->ppObj->getRelations($this->pluginPackId, OperationManager::STPLf);
        if (!array_key_exists($serviceId, $relations)) {
            $this->ppObj->setRelations($this->pluginPackId, OperationManager::STPL, $ppServiceTemplates);
        }

        $this->logMessage("Updated service : " . $tmpSvc["service_description"]);

        return true;
    }

    private function getServiceTplIdByDescription($serviceDescription)
    {
        $sql = "SELECT service_id 
            FROM service 
            WHERE service_description = '" . $this->db->escape($serviceDescription) . "'
            AND service_register = '0'";
        $res = $this->db->query($sql);
        if ($res->numRows() == 1) {
            $row = $res->fetchRow();
            return $row['service_id'];
        } else if ($res->numRows() > 0) {
            throw new \Exception('More than one service template found in DB with name ' . $serviceDescription);
        } else {
            throw new \Exception('No service template found in DB with name ' . $serviceDescription);
        }
        return '';
    }

    /**
     * Get all columns for service table
     *
     * @return $columns array
     */
    public function getServiceColumns()
    {
        $columns = array();

        $query = "desc service";
        $dbres = $this->db->query($query);
        while ($row = $dbres->fetchRow()) {
            $columns[$row['Field']] = 1;
        }
        return $columns;
    }

    /**
     * Get all columns for host table
     *
     * @return $columns array
     */
    public function getHostColumns()
    {
        $columns = array();

        $query = "desc host";
        $dbres = $this->db->query($query);
        while ($row = $dbres->fetchRow()) {
            $columns[$row['Field']] = 1;
        }
        return $columns;
    }

    /**
     * Insert service
     *
     * @param array $tmpSvc
     * @param CentreonDB $this->db
     * @return int
     */
    public function insertService($tmpSvc, $params)
    {
        $templateName = "";

        /* Get centreon.service columns */
        $columns = $this->getServiceColumns();

        $startReq = "INSERT INTO service (service_id, ";
        $endReq = " VALUES (NULL, ";
        $count = 0;
        $ppServiceTemplates = array();
        foreach ($tmpSvc as $key => $val) {
            // We check that columns listed as tags in XML exist en DB otherwise we will generate an invalid SQL request
            // This check is needed as PP have to work for 2 versions of Centreon at the same time, and SQL schema may differ
            // Other keys are XML tags that are not mapped to host table columns (see below specific associated code)
            // FIXME Temporary debug
            /* if (!isset($columns[$key])) {
              $this->logMessage("Warning : XML tag does not exist in service table : $key");
              } */
            if (isset($columns[$key]) || $key == 'macros' || $key == "traprelations" || $key == "service_template_name") {
                if ($key == 'service_template_name') {
                    $templateName = $val;
                    continue;
                }
                if ($key == "service_id") {
                    continue;
                }
                if ($key == "macros") {
                    $macros = $val;
                    continue;
                }
                if ($key == "traprelations") {
                    $trapRelations = $val;
                    continue;
                }
                $val = trim($val);
                if ($count) {
                    $startReq .= ", ";
                }
                $startReq .= $key;
                if ($count) {
                    $endReq .= ", ";
                }
                if ($key == "service_template_model_stm_id") {
                    if (isset($this->translationId['svc'][$val])) {
                        $val = $this->translationId['svc'][$val];
                    } else {
                        // Inherit from another template (that may belong to the same RPM or not)
                        // We are looking for the associated TPL id
                        // For all PP, service TPL MUST inherit from "generic-active-service" which is provided by ces-pack base RPM
                        // so we're checking that we are inheriting from an existing template and raise an error exept if it is generic-active-service himself
                        if ($tmpSvc['service_description'] != 'generic-active-service') {
                            if (isset($params['service_template_name']) && $params['service_template_name'] != '') {
                                if ($params['service_template_name'] == 'generic-active-service' || preg_match('/-custom$/i', $params['service_template_name'])) {
                                    $val = $this->getServiceTplIdByDescription($params['service_template_name']);
                                } else {
                                    throw new \Exception("Parent template of '" . $tmpSvc['service_description'] . "' is '" . $params['service_template_name'] . "', it must be a '*-custom'. Invalid template.xml.\n");
                                }
                            } else {
                                throw new \Exception("No parent template defined for template '" . $tmpSvc['service_description'] . "'. Invalid template.xml.\n");
                            }
                        }
                    }
                }
                if (($key == "command_command_id" || $key == "command_command_id2") && isset($this->translationId['cmd'][$val])) {
                    $val = $this->translationId['cmd'][$val];
                }
                if (($key == "timeperiod_tp_id" || $key == "timeperiod_tp_id2") && isset($this->translationId['tp'][$val])) {
                    $val = $this->translationId['tp'][$val];
                }
                $endReq .= ($val == "") ? "NULL" : "'" . $this->db->escape($val) . "'";
                $count++;
            }
        }
        $startReq .= ")";
        $endReq .= ")";
        $query = $startReq . $endReq;
        $this->db->query($query);

        $res = $this->db->query("SELECT service_id FROM service WHERE service_description LIKE '" . $tmpSvc["service_description"] . "' AND service_register = '0'");
        if ($res->numRows()) {
            $data = $res->fetchRow();
            $service_id = $data["service_id"];
            $request = "INSERT INTO extended_service_information (esi_id, service_service_id) VALUES (NULL, '" . $service_id . "')";
            $ppServiceTemplates[] = $service_id;
            $this->db->query($request);
            if (isset($macros)) {
                $this->updateMacros($macros, $service_id, "service");
            }
            if (isset($trapRelations)) {
                $this->updateServiceTrapRelations($trapRelations, $service_id);
            }
        } else {
            throw new \Exception("Insert problem ($query)");
        }
        if (count($ppServiceTemplates)) {
            $this->ppObj->setRelations(
                $this->pluginPackId, OperationManager::STPL, $ppServiceTemplates
            );
        }
        $this->logMessage("Inserted new service : " . $tmpSvc["service_description"]);
        return $service_id;
        ;
    }

    /**
     * Update macros
     * Returns true if update is made, false otherwise
     *
     * @param array $macros
     * @param int $objectId
     * @param string $type
     * @throws Exception
     * @return void
     */
    public function updateMacros($macros, $objectId, $type = "host")
    {
        $this->logMessage("Processing macros for $type ($objectId)");

        if ($type == "host") {
            $fk = "host_host_id";
            $typeColomn = "host";
        } elseif ($type == "service") {
            $fk = "svc_svc_id";
            $typeColomn = "svc";
        } else {
            throw new \Exception("incorrect macro type");
        }
        $query = "DELETE FROM on_demand_macro_$type WHERE $fk = " . $this->db->escape($objectId);
        $this->db->query($query);
        $query = "INSERT INTO on_demand_macro_$type (" . $typeColomn . "_macro_name, " . $typeColomn . "_macro_value, $fk) VALUES ";
        $str = "";
        foreach ($macros as $macro) {
            $this->logMessage("Processing macro : '" . $macro['key'] . "' => '" . $macro['value'] . "'");
            if ($str != "") {
                $str .= ", ";
            }
            $str .= "('" . $this->db->escape($macro['key']) . "', '" . $this->db->escape($macro['value']) . "', $objectId)";
        }
        if ($str != "") {
            $this->db->query($query . $str);
        }
        $this->logMessage("Inserted $type macros");
        return true;
    }

    /**
     * Update host
     * Returns true if update is made, false otherwise
     *
     * @param int $hostId
     * @param array $tmpHost
     * @return bool
     */
    public function updateHost($hostId, $tmpHost)
    {
        $query = "UPDATE host SET ";
        $condition = " WHERE host_id = " . $this->db->escape($hostId);
        $count = 0;
        $ehiQuery = "UPDATE extended_host_information SET ";
        $ehiCondition = " WHERE host_host_id = " . $this->db->escape($hostId);
        $ehiCount = 0;
        foreach ($tmpHost as $key => $val) {
            if ($key == "macros") {

                $this->updateMacros($val, $hostId, "host");
            } elseif ($key != 'host_id' && $key != 'host_snmp_community' && $key != 'host_snmp_version' && !preg_match("/^ehi/", $key)) {
                $val = trim($val);

                if (($key == "command_command_id" || $key == "command_command_id2") && isset($this->translationId["cmd"][$val])) {
                    $val = $this->translationId["cmd"][(int) $val];
                }
                if (($key == "timeperiod_tp_id" || $key == "timeperiod_tp_id2") && isset($this->translationId["tp"][$val])) {
                    $val = $this->translationId["tp"][(int) $val];
                }
                if ($val != "") {
                    $val = "'" . $this->db->escape($val) . "'";
                } else {
                    $val = "NULL";
                }
                if ($count) {
                    $query .= ", ";
                }
                $query .= $key . " = " . $val;
                $count++;
                // Process extended host information
            } elseif (preg_match("/^ehi/", $key) && $key != "ehi_id") {

                if ($val != "") {
                    $val = "'" . $this->db->escape($val) . "'";
                } else {
                    $val = "NULL";
                }
                // We do not support icons for now
                if ($key != "ehi_icon_image" && $key != "ehi_vrml_image" && $key != "ehi_statusmap_image") {
                    if ($ehiCount) {
                        $ehiQuery .= ", ";
                    }
                    $ehiQuery .= $key . " = " . $val;
                    $ehiCount++;
                }
            }
        }
        if ($count) {
            $query .= $condition;
            $this->db->query($query);
        }
        if ($ehiCount) {
            $query .= $ehiCondition;
            $this->db->query($ehiQuery);
        }
        
        $ppHostTemplates[] = $hostId;
        $this->ppObj->setRelations(
            $this->pluginPackId, OperationManager::HTPL, $ppHostTemplates
        );
           
        $this->logMessage("Updated host : " . $tmpHost['host_name']);
        return true;
    }

    /**
     * Insert Host
     *
     * @param array $tmpHost
     * @param CentreonDB $this->db
     * @return int
     */
    public function insertHost($tmpHost)
    {

        /* Get centreon.host columns */
        $columns = $this->getHostColumns();

        $startReq = "INSERT INTO host (";
        $endReq = " VALUES (";
        $count = 0;
        $ppHostTemplates = array();
        foreach ($tmpHost as $key => $val) {
            // FIXME Temporary debug
            /* if (!isset($columns[$key])) {
              $this->logMessage("Warning : XML tag does not exist in host table : $key");
              } */

            // See insertService() for more info
            if (isset($columns[$key]) || $key == 'macros') {
                if ($key == "macros") {
                    $macros = $val;
                    // ehi = extended host information, processed later
                } elseif ($key != "host_id" && !preg_match("/^ehi/", $key)) {
                    $val = trim($val);
                    if ($count) {
                        $startReq .= ", ";
                    }
                    $startReq .= "$key";
                    if ($count) {
                        $endReq .= ", ";
                    }
                    if (($key == "command_command_id" || $key == "command_command_id2") && isset($this->translationId["cmd"][$val])) {
                        $val = $this->translationId["cmd"][$val];
                    }
                    if (($key == "timeperiod_tp_id" || $key == "timeperiod_tp_id2") && isset($this->translationId["tp"][$val])) {
                        $val = $this->translationId["tp"][$val];
                    }
                    $endReq .= ($val == "" ? "NULL" : "'" . $this->db->escape($val) . "'");
                    $count++;
                }
            }
        }
        $startReq .= ")";
        $endReq .= ")";
        $request = $startReq . $endReq;
        $this->db->query($request);

        $res = $this->db->query("SELECT host_id FROM host WHERE host_name LIKE '" . $tmpHost["host_name"] . "' AND host_register = '0'");
        if ($res->numRows()) {
            $data = $res->fetchRow();
            $host_id = $data["host_id"];
            $ppHostTemplates[] = $host_id;
            $request = "INSERT INTO extended_host_information (ehi_id, host_host_id) VALUES (NULL, '" . $host_id . "')";
            $this->db->query($request);

            // Process extended host information
            foreach ($tmpHost as $key => $val) {
                // We do not support icons for now
                if (preg_match("/^ehi/", $key) && $key != 'ehi_id' && $key != 'ehi_icon_image' && $key != 'ehi_vrml_image' && $key != 'ehi_statusmap_image') {
                    $val = trim($val);
                    $request = "UPDATE extended_host_information SET $key = '" . $this->db->escape($val) . "' WHERE host_host_id = '$host_id'";
                    $this->db->query($request);
                    $count++;
                }
            }

            if (isset($macros)) {
                $this->updateMacros($macros, $host_id, "host");
            }

            if (count($ppHostTemplates)) {
                $this->ppObj->setRelations(
                    $this->pluginPackId, OperationManager::HTPL, $ppHostTemplates
                );
            }

            $this->logMessage("Inserted new host : " . $tmpHost['host_name']);
            return $host_id;
        } else {
            $this->logMessage("ERROR - Could not insert host : " . $tmpHost['host_name']);
            return 0;
        }
    }

    /**
     * Clear service template from host template
     *
     * @param string $hostName
     */
    public function clearSvcTplFromHostTpl($hostName)
    {
        $query = "DELETE FROM host_service_relation
              WHERE service_service_id IS NOT NULL
              AND host_host_id IN (SELECT host_id
                                     FROM host
                                     WHERE host_name = '" . $this->db->escape($hostName) . "'
                                     AND host_register = '0')";
        $this->db->query($query);
    }

    /**
     * Clear host template relation from host template
     *
     * @param string $hostName
     */
    public function clearRelationsFromHostTpl($hostName)
    {
        $query = "DELETE FROM host_template_relation
              WHERE host_host_id IN (SELECT host_id
                                       FROM host
                                       WHERE host_name = '" . $this->db->escape($hostName) . "'
                                       AND host_register = '0')";
        $this->db->query($query);
    }

    /**
     * Log Message
     *
     * @param string $msg
     * @return void
     * errorType $id 1 = "login.log";
     * errorType $id 2 = "sql-error.log";
     * errorType $id 3 = "ldap.log";
     * errotType $id 4 = "pluginspacks.log"
     * 
     * print to screen $scr = 1 (by default no output)
     */
    public function logMessage($msg, $scr = 0, $id = 4)
    {
        $this->logs->insertLog($id, $msg, $scr);
    }

    /**
     * Insert Command
     *
     * @param array $params
     * @return void
     */
    public function insertCommand($params)
    {
        $rq = "INSERT INTO `command` (`command_name`, `command_line`, `command_example`, `command_type`, `graph_id`) ";
        $rq .= "VALUES ('" . $this->db->escape($params["command_name"]) . "', '" . $this->db->escape($params["command_line"]) . "', '" . $this->db->escape($params["command_example"]) . "', '" . $params["command_type"] . "', '" . $params["graph_id"] . "')";
        $this->db->query($rq);
        $res = $this->db->query("SELECT MAX(command_id) as last_id
            FROM command 
            WHERE command_name = '{$this->db->escape($params['command_name'])}'");
        $row = $res->fetchRow();
        $this->ppObj->setRelations(
            $this->pluginPackId, OperationManager::CMD, array($row['last_id'])
        );
        $this->logMessage("New command inserted : " . $params['command_name']);
    }

    /**
     * Update Command
     * Returns true if update can be made, false otherwise
     *
     * @param int $cmdId
     * @param array $params
     * @return bool
     */
    public function updateCommand($cmdId, $params)
    {
        $query = "UPDATE command SET ";
        $condition = " WHERE command_id = " . $this->db->escape($cmdId);
        $count = 0;
        foreach ($params as $key => $val) {
            if ($key != "command_id" && $key != "command_name") {
                if ($count) {
                    $query .= ", ";
                }
                if ($val != "") {
                    $val = "'" . $this->db->escape($val) . "'";
                } else {
                    $val = "NULL";
                }
                $query .= $key . " = " . $val;
                $count++;
            }
        }
        if ($count) {
            $query .= $condition;
            $this->db->query($query);
            // Update relation even if command has been created by an other plugin
            $ppCmd[] = $cmdId;
            $relations = $this->ppObj->getRelations($this->pluginPackId, OperationManager::CMD);
            if (!array_key_exists($cmdId, $relations)) {
                $this->ppObj->setRelations(
                    $this->pluginPackId, OperationManager::CMD, $ppCmd
                );
            }
            $this->logMessage("Command updated : " . $params['command_name']);
        }
        return true;
    }

    /**
     * Insert Time Period
     *
     * @param array $params
     * @return void
     */
    public function insertTimePeriod($params)
    {
        $rq = "INSERT INTO timeperiod ";
        $rq .= "(tp_name, tp_alias, tp_sunday, tp_monday, tp_tuesday, tp_wednesday, tp_thursday, tp_friday, tp_saturday) ";
        $rq .= "VALUES (";
        isset($params["tp_name"]) ? $rq .= "'" . $this->db->escape($params["tp_name"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_alias"]) ? $rq .= "'" . $this->db->escape($params["tp_alias"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_sunday"]) ? $rq .= "'" . $this->db->escape($params["tp_sunday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_monday"]) ? $rq .= "'" . $this->db->escape($params["tp_monday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_tuesday"]) ? $rq .= "'" . $this->db->escape($params["tp_tuesday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_wednesday"]) ? $rq .= "'" . $this->db->escape($params["tp_wednesday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_thursday"]) ? $rq .= "'" . $this->db->escape($params["tp_thursday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_friday"]) ? $rq .= "'" . $this->db->escape($params["tp_friday"]) . "', " : $rq .= "NULL, ";
        isset($params["tp_saturday"]) ? $rq .= "'" . $this->db->escape($params["tp_saturday"]) . "'" : $rq .= "NULL";
        $rq .= ")";
        $this->db->query($rq);
        $this->logMessage("New timeperiod inserted : " . $params['tp_name']);
    }

    /**
     * Update Time Period
     * Returns true if update can be made, false otherwise
     *
     * @param int $tpId
     * @param array $params
     * @return bool
     */
    public function updateTimePeriod($tpId, $params)
    {
        $query = "UPDATE timeperiod SET ";
        $condition = " WHERE tp_id = " . $this->db->escape($tpId);
        $count = 0;
        foreach ($params as $key => $val) {
            if ($key != "tp_id" && $key != "tp_name") {
                if ($count) {
                    $query .= ", ";
                }
                if ($val != "") {
                    $val = "'" . $this->db->escape($val) . "'";
                } else {
                    $val = "NULL";
                }
                $query .= $key . " = " . $val;
                $count++;
            }
        }
        if ($count) {
            $query .= $condition;
            $this->db->query($query);
            $this->logMessage("Timeperiod updated : " . $params['tp_name']);
        }
        return true;
    }

    /**
     * Insert vendor
     *
     * @param array $params
     * @return int
     */
    public function insertVendor($params)
    {
        $startReq = "INSERT INTO traps_vendor (";
        $endReq = " VALUES (";
        $count = 0;
        foreach ($params as $key => $val) {
            if ($key != "id") {
                $val = trim($val);
                if ($count) {
                    $startReq .= ", ";
                }
                $startReq .= "$key";
                if ($count) {
                    $endReq .= ", ";
                }
                $endReq .= ($val == "" ? "NULL" : "'" . $this->db->escape($val) . "'");
                $count++;
            }
        }
        $startReq .= ")";
        $endReq .= ")";
        $request = $startReq . $endReq;
        $this->db->query($request);

        $res = $this->db->query("SELECT id FROM traps_vendor WHERE name LIKE '" . $params["name"] . "'");
        if ($res->numRows()) {
            $data = $res->fetchRow();
            $vendorId = $data["id"];
            $this->logMessage("Inserted new vendor : " . $params['name']);
            return $vendorId;
        } else {
            $this->logMessage("ERROR - Could not insert vendor : " . $params['name']);
            return 0;
        }
    }

    /**
     * Update vendor
     *
     * @param int $vendorId
     * @param array $params
     * @return bool
     */
    public function updateVendor($vendorId, $params)
    {
        $query = "UPDATE traps_vendor SET ";
        $condition = " WHERE id = " . $this->db->escape($vendorId);
        $count = 0;
        foreach ($params as $key => $val) {
            if ($key != "id") {
                $val = trim($val);
                if ($count) {
                    $query .= ", ";
                }
                if ($val != "") {
                    $val = "'" . $val . "'";
                } else {
                    $val = "NULL";
                }
                $query .= $key . " = " . $this->db->escape($val);
                $count++;
            }
        }
        if ($count) {
            $query .= $condition;
            $this->db->query($query);
        }
        $this->logMessage("Updated vendor : " . $params['name']);
        return true;
    }

    /**
     * Insert Trap
     *
     * @param array $params
     * @return int
     */
    public function insertTrap($params)
    {
        $startReq = "INSERT INTO traps (";
        $endReq = " VALUES (";
        $count = 0;
        $startReqTmo = "INSERT INTO traps_matching_properties (";
        $endReqTmo = " VALUES (";
        $countTmo = 0;
        foreach ($params as $key => $val) {
            if ($key != "traps_id" && $key != "trap_id" && !preg_match("/^tmo/", $key)) {
                $val = trim($val);
                if ($count) {
                    $startReq .= ", ";
                }
                $startReq .= "$key";
                if ($count) {
                    $endReq .= ", ";
                }
                if ($val == "") {
                    $endReq .= "NULL";
                } elseif ($key == "manufacturer_id") {
                    $endReq .= $this->translationId['vendor'][$val];
                } else {
                    $endReq .= "'" . $this->db->escape($val) . "'";
                }
                $count++;
            } elseif ($key != "tmo_id" && preg_match("/^tmo/", $key)) {
                $val = trim($val);
                if ($countTmo) {
                    $startReqTmo .= ", ";
                }
                $startReqTmo .= "$key";
                if ($countTmo) {
                    $endReqTmo .= ", ";
                }
                $endReqTmo .= ($val == "" ? "NULL" : "'" . $this->db->escape($val) . "'");
                $countTmo++;
            }
        }
        $startReq .= ")";
        $endReq .= ")";
        $startReqTmo .= ")";
        $endReqTmo .= ")";
        $request = $startReq . $endReq;
        $this->db->query($request);

        $res = $this->db->query("SELECT traps_id FROM traps WHERE traps_name LIKE '" . $params["traps_name"] . "'");
        if ($res->numRows()) {
            $data = $res->fetchRow();
            $trapId = $data["traps_id"];
            if ($countTmo) {
                $request = $startReqTmo . $endReqTmo;
                $this->db->query($request);
            }
            $this->logMessage("Inserted new trap : " . $params['traps_name']);
            return $trapId;
        } else {
            $this->logMessage("ERROR - Could not insert trap : " . $params['traps_name']);
            return 0;
        }
    }

    /**
     * Update Trap
     *
     * @param int $trapId
     * @param array $params
     * @return bool
     */
    public function updateTrap($trapId, $params)
    {
        $query = "UPDATE traps SET ";
        $condition = " WHERE traps_id = " . $this->db->escape($trapId);
        $count = 0;
        $startReqTmo = "INSERT INTO traps_matching_properties (";
        $endReqTmo = " VALUES (";
        $countTmo = 0;
        foreach ($params as $key => $val) {
            if ($key != "traps_id" && $key != "trap_id" && !preg_match("/^tmo/", $key)) {
                $val = trim($val);
                if ($count) {
                    $query .= ", ";
                }
                if ($val != "") {
                    $val = "'" . $val . "'";
                } else {
                    $val = "NULL";
                }
                $query .= $key . " = " . $this->db->escape($val);
                $count++;
            } elseif (preg_match("/^tmo/", $key) && $key != "tmo_id") {
                $val = trim($val);
                if ($countTmo) {
                    $startReqTmo .= ", ";
                }
                $startReqTmo .= "$key";
                if ($countTmo) {
                    $endReqTmo .= ", ";
                }
                $endReqTmo .= ($val == "" ? "NULL" : "'" . $this->db->escape($val) . "'");
                $countTmo++;
            }
        }
        if ($count) {
            $query .= $condition;
            $this->db->query($query);
        }
        $startReqTmo .= ")";
        $endReqTmo .= ")";
        if ($countTmo) {
            $this->db->query("DELETE FROM traps_matching_properties WHERE trap_id = " . $this->db->escape($trapId));
            $request = $startReqTmo . $endReqTmo;
            $this->db->query($request);
        }
        $this->logMessage("Updated trap : " . $params['traps_name']);
        return true;
    }

    /**
     * Set Translation Id
     *
     * @param string $section
     * @param int $id
     * @param int $realId
     * @return void
     */
    public function setTranslationId($section, $id, $realId)
    {
        $id = (int) $id;
        $realId = (int) $realId;
        $this->translationId[$section][$id] = $realId;
    }

    /**
     * Update Service Trap Relations
     *
     * @param array $traps
     * @param int $serviceId
     * @throws Exception
     * @return void
     */
    public function updateServiceTrapRelations($traps, $serviceId)
    {
        $query = "DELETE FROM traps_service_relation WHERE service_id = " . $this->db->escape($serviceId);
        $this->db->query($query);
        $query = "INSERT INTO traps_service_relation (traps_id, service_id) VALUES ";
        $str = "";
        foreach ($traps as $trap) {
            if ($str != "") {
                $str .= ", ";
            }
            $trapId = trim($trap['id']);
            if (isset($this->translationId['trap'][$trapId])) {
                $realTrapId = $this->translationId['trap'][$trapId];
            }
            if (!isset($realTrapId)) {
                throw new \Exception("No translation id found for trap : " . $trap['name']);
            }
            $str .= "(" . $this->db->escape($realTrapId) . ", " . $this->db->escape($serviceId) . ")";
        }
        if ($str != "") {
            $this->db->query($query . $str);
            $this->logMessage("Service trap relations updated");
        }
    }
}
