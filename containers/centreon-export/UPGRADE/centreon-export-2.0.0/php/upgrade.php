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

/**
 * require configuration files
 */
if (file_exists(_CENTREON_ETC_ . '/centreon.conf.php')) {
    include_once _CENTREON_ETC_ . '/centreon.conf.php';
}

require_once realpath(dirname(__FILE__) . '/../../../centreon-export.conf.php');

use \CentreonExport\Import;
use \CentreonExport\CentreonClapiDB;

define("CLAPI_APPLICATION_PATH", _CENTREON_PATH_ . "/www/modules/centreon-clapi/core/");
set_include_path(
    implode(PATH_SEPARATOR, 
        array(
            realpath(CLAPI_APPLICATION_PATH . '/lib'), 
            realpath(CLAPI_APPLICATION_PATH . '/class'), 
            get_include_path()
        )
    )
);

$dir = "/srv/plugins-packs-repositories/";

$permissions = substr(sprintf('%o', fileperms($dir)), -4);
chmod($dir, 0777);
if (is_dir($dir)) {
    if ($dh = opendir($dir)) {
        while ($elem = readdir($dh)) {
            if (is_dir($dir . $elem) && $elem != "." && $elem != ".." && $elem != "Centreon") {
                $sConfXml = $dir . $elem . "/templates/plugins-pack.xml";
                upgrade($sConfXml);
            }
        }
    }
}
chmod($dir, $permissions);

/**
 * Get service template id
 * 
 * @param string $serviceDescription
 * @return int
 */
function getServiceTplID($db, $serviceDescription)
{
    $sql = "SELECT service_id 
            FROM service 
        WHERE service_description = '" . $db->escape($serviceDescription) . "'
        AND service_register = '0'
        LIMIT 1";
    $res = $db->query($sql);
    if ($res->numRows()) {
        $row = $res->fetchRow();
        return $row['service_id'];
    }
    return 0;
}

/**
 * Get host template id
 * 
 * @param string $hostName
 * @return int
 */
function getHostTplID($db, $hostName)
{
    $sql = "SELECT host_id 
            FROM host 
            WHERE host_name = '" . $db->escape($hostName) . "'
            AND host_register = '0'
            LIMIT 1";
    $res = $db->query($sql);
    if ($res->numRows()) {
        $row = $res->fetchRow();
        return $row['host_id'];
    }
    return 0;
}

function upgrade($sFileXml)
{
    $db = new CentreonClapiDB();
    $db->autocommit();
    
    $tab = stat($sFileXml);
    $lastupdate = $tab[9];


    $import = new Import($db, $sFileXml);

    try {
        $xmlString = $import->getXmlObject();
        $import->logMessage("File processing starting");

        /**
         * Plugin pack info
         */
        foreach ($xmlString->information as $info) {
            if (isset($info->name) && isset($info->version)) {
                $ppName = $info->name;
                $ppVersion = $info->version;
                $ppRelease = $info->release;
                $ppStatus = $info->status;
                $ppStatusMessage = $info->status_message;
            }
        }
        if (!isset($ppName)) {
            throw new \Exception('Missing plugin pack name in XML');
        }
        if (!isset($ppVersion)) {
            throw new \Exception('Missing plugin pack version in XML');
        }
        if (!isset($ppRelease)) {
            throw new \Exception('Missing plugin pack release in XML');
        }
        $import->initInstall($ppName, $ppVersion, $ppRelease, $ppStatus, $ppStatusMessage, $lastupdate);

        /**
         * Commands
         */
        foreach ($xmlString->commands as $command) {
            foreach ($command as $key => $value) {
                $params = array();
                foreach ($value->children() as $key => $str) {
                    $params[$key] = $str;
                }
                $res = $db->query("SELECT command_id FROM command WHERE command_name LIKE '" . $params["command_name"] . "'");

                if ($res->numRows()) {
                    $data = $res->fetchRow();
                    $import->setTranslationId("cmd", $params["command_id"], $data['command_id']);
                    $import->updateCommand($data['command_id'], $params);
                } else {
                    $import->insertCommand($params);
                    $res = $db->query("SELECT command_id FROM command WHERE command_name LIKE '" . $params["command_name"] . "'");
                    if ($res->numRows()) {
                        $data = $res->fetchRow();
                        $import->setTranslationId("cmd", $params["command_id"], $data['command_id']);
                    }
                }
            }
        }

        /**
         * Timeperiods
         */
        foreach ($xmlString->timeperiods as $timeperiod) {
            foreach ($timeperiod as $key => $value) {
                $params = array();
                foreach ($value->children() as $key => $str) {
                    $params[$key] = $str;
                }
                $res = $db->query("SELECT tp_id FROM timeperiod WHERE tp_name LIKE '" . $params["tp_name"] . "'");
                if ($res->numRows()) {
                    $data = $res->fetchRow();
                    $import->setTranslationId("tp", $params['tp_id'], $data['tp_id']);
                    $import->updateTimePeriod($data["tp_id"], $params);
                } else {
                    $import->insertTimePeriod($params);
                    $res = $db->query("SELECT tp_id FROM timeperiod WHERE tp_name LIKE '" . $params["tp_name"] . "'");
                    if ($res->numRows()) {
                        $data = $res->fetchRow();
                        $import->setTranslationId("tp", $params['tp_id'], $data['tp_id']);
                    }
                }
            }
        }

        /**
         * Manufacturers
         */
        foreach ($xmlString->manufacturers as $manufacturer) {
            foreach ($manufacturer as $key => $value) {
                $params = array();
                foreach ($value->children() as $key => $str) {
                    $params[$key] = $str;
                }
                $res = $db->query("SELECT id FROM traps_vendor WHERE name LIKE '" . $params['name'] . "'");
                if ($res->numRows()) {
                    $data = $res->fetchRow();
                    $import->setTranslationId("vendor", $params['id'], $data['id']);
                    $import->updateVendor($data['id'], $params);
                } else {
                    $vendorId = $import->insertVendor($params);
                    $import->setTranslationId("vendor", $params['id'], $vendorId);
                }
            }
        }

        /**
         * Traps
         */
        foreach ($xmlString->traps as $trap) {
            foreach ($trap as $key => $value) {
                $params = array();
                foreach ($value->children() as $key => $str) {
                    $params[$key] = $str;
                }
                $res = $db->query("SELECT traps_id FROM traps WHERE traps_name LIKE '" . $params['traps_name'] . "'");
                if ($res->numRows()) {
                    $data = $res->fetchRow();
                    $import->setTranslationId("trap", $params['traps_id'], $data['traps_id']);
                    $import->updateTrap($data['traps_id'], $params);
                } else {
                    $trapId = $import->insertTrap($params);
                    $import->setTranslationId("trap", $params['traps_id'], $trapId);
                }
            }
        }

        /**
         * Templates
         */
        foreach ($xmlString->templates as $tpls) {
            foreach ($tpls->children() as $tpl) {
                $params = array();
                foreach ($tpl->children() as $key => $str) {
                    $params[$key] = $str;
                }
                $s = $tpl->attributes();
                if ($svcId = getServiceTplID($db, $s['service_description'])) {
                    $import->updateService($svcId, $tpl, $params);
                    $import->setTranslationId("svc", $params["service_id"], $svcId);
                } else {
                    $svcTplId = $import->insertService($tpl, $params);
                    $import->setTranslationId("svc", $params["service_id"], getServiceTplID($db, $s['service_description']));
                }
            }
        }

        /**
         * Hosts
         */
        foreach ($xmlString->hosts as $host) {
            foreach ($host->children() as $h) {
                $svcTplId = array();
                $relationId = array();
                $tmpHost = array();

                $import->clearRelationsFromHostTpl($h['host_name']);
                $import->clearSvcTplFromHostTpl($h['host_name']);
                foreach ($h->children() as $key => $str) {
                    if ($key == "services") {
                        foreach ($str->children() as $svc) {
                            $s = $svc->attributes();
                            $params = array();
                            foreach ($svc->children() as $key => $str) {
                                $params[$key] = $str;
                            }
                            if ($i = getServiceTplID($db, $s['service_description'])) {
                                $svcTplId[] = $i;
                                $import->updateService($i, $svc, $params);
                            } else {
                                $svcTplId[] = $import->insertService($svc, $params);
                            }
                        }
                    } elseif ($key == "relations") {
                        foreach ($str->children() as $relation) {
                            $r = $relation->attributes();
                            if ($i = getHostTplID($db, $r['value'])) {
                                $relationId[] = $i;
                            }
                        }
                    } else {
                        $tmpHost[$key] = $str;
                    }
                }
                if ($tmpHost["host_register"] == 0) {
                    if ($hostId = getHostTplID($db, $tmpHost["host_name"])) {
                        $import->updateHost($hostId, $tmpHost);
                        $import->setTranslationId("host", $tmpHost["host_id"], $hostId);
                    } else {
                        $hostId = $import->insertHost($tmpHost);
                        $import->setTranslationId("host", $tmpHost["host_id"], $hostId);
                    }
                    $import->insertServiceRelations($svcTplId, $hostId);
                    $import->insertHostRelations($relationId, $hostId);
                }
            }
        }
        $import->logMessage("File processing finished.");
        $db->commit();
    } catch (\Exception $e) {
        $db->rollback();
        $import->logMessage("ERROR : " . $e->getMessage());
    }
}

?>
