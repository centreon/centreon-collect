<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */

require_once "Centreon/Object/Object.php";
require_once dirname(__FILE__) . "/../../common/functions.php";

/**
 * Description of BA
 *
 * @author tmechouet
 */
class BA extends Centreon_Object {
    protected $table = "mod_bam";
    protected $primaryKey = "ba_id";
    protected $uniqueLabelField = "name";
    
    
    /**
     * Used for inserting ba into database
     *
     * @param array $params
     * @return int
     */
    public function insert($parameters)
    {
        $id = parent::insert($parameters);
        /* Get localhost poller */
        $query = "SELECT id FROM nagios_server WHERE localhost = '1'";
        $res = $this->db->query($query);
        $row = $res->fetch();
        
        /* Get or create the virtual host for BA */
        $hostId = $this->getCentreonBaHostId($row['id']);
        
        /* Get or create the virtual service for BA */
        $this->getCentreonBaServiceId('ba_' . $id, $hostId);
        
        $this->updatePoller($id);
        return $id;
    }
    
    /**
     * Get object parameters
     *
     * @param int $objectId
     * @param mixed $parameterNames
     * @return array
     */
    public function getParameters($objectId, $parameterNames)
    {
	$params = Centreon_Object::getParameters($objectId, $parameterNames);
	$params_image = array("icon_id");
	foreach ($params_image as $image) {
  	    if (array_key_exists($image, $params)) {
	        $sql = "SELECT dir_name,img_path 
                        FROM view_img vi 
                        LEFT JOIN view_img_dir_relation vidr ON vi.img_id = vidr.img_img_id 
                        LEFT JOIN view_img_dir vid ON vid.dir_id = vidr.dir_dir_parent_id 
                        WHERE img_id = ?";
                $res = $this->getResult($sql, array($params[$image]), "fetch");
		if (is_array($res)) {
                    $params[$image] = $res["dir_name"]."/".$res["img_path"];
                }
            }
        }
        
        return $params;
    }
    
    /**
     * Update the additional poller if set
     *
     * @param int $baId The business activity ID
     * @param int $iIdPoller The additionnal poller to link
     */
    public function updatePoller($baId, $iIdPoller = null)
    { 
        $this->db->query("DELETE FROM mod_bam_poller_relations WHERE ba_id = " . $baId);

        $this->db->query("INSERT INTO mod_bam_poller_relations(ba_id, poller_id)
            SELECT " . $baId . ", id FROM nagios_server WHERE localhost = '1'");
        
        if (false === is_null($iIdPoller)) {
            $this->db->query("INSERT INTO mod_bam_poller_relations (ba_id, poller_id) VALUES (" . $baId . ", " . (int) $iIdPoller. ")");

            /* Get or create the virtual host for BA */
            $hostId = $this->getCentreonBaHostId($iIdPoller);
            
            /* Get or create the virtual service for BA */
            $this->getCentreonBaServiceId('ba_' . $baId, $hostId);
        }
    }
    
    /**
     * Get Centreon BA Service Id
     *
     * @param string $metaName
     * @param int $hostId | bam host id
     * @return int
     */
    protected function getCentreonBaServiceId($baName, $hostId = null)
    {
        try {
            $query = "SELECT service_id FROM service WHERE service_register = '2' AND service_description = ".$this->db->quote($baName);
            $res = $this->db->query($query);
            $sid = null;
            if (!$res->rowCount()) {
                $query = "INSERT INTO service (service_description, service_register) VALUES (".$this->db->quote($baName).", '2')";
                $query = "INSERT INTO service (service_description, service_register) VALUES (".$this->db->quote($baName).", '2')";
                $this->db->query($query);
                $query = "SELECT MAX(service_id) as sid FROM service WHERE service_description = ".$this->db->quote($baName)." AND service_register = '2'";
                $resId = $this->db->query($query);
                if ($resId->rowCount()) {
                    $row = $resId->fetch();
                    $sid = $row['sid'];
                }
            } else {
                 $row = $res->fetch();
                 $sid = $row['service_id'];
            }
            if (!isset($sid)) {
                throw new Exception('Service id of BAM Module could not be found');
            }
            if (false === is_null($hostId)) {
                $this->db->query("DELETE FROM host_service_relation WHERE host_host_id = ".$this->db->quote($hostId)." AND service_service_id = ".$this->db->quote($sid));
                $this->db->query("INSERT INTO host_service_relation (host_host_id, service_service_id) VALUES (".$this->db->quote($hostId).", ".$this->db->quote($sid).")");
            }
            return $sid;
        } catch (Exception $e) {
            echo $e->getMessage() . "<br/>";
        }
    }

    /**
     * Get Centreon BA Host Id
     *
     * @param int $pollerId The poller for host
     * @return int
     */
    protected function getCentreonBaHostId($pollerId)
    {
        try {
            $query = "SELECT host_id FROM host WHERE host_register = '2' AND host_name = '_Module_BAM_" . $pollerId . "'";
            $res = $this->db->query($query);
            $hid = null;
            if (!$res->rowCount()) {
                $query = "INSERT INTO host (host_name, host_register) VALUES ('_Module_BAM_" . $pollerId . "', '2')";
                $this->db->query($query);
                $query = "SELECT MAX(host_id) as hid FROM host WHERE host_name = '_Module_BAM_" . $pollerId . "' AND host_register = '2'";
                $resId = $this->db->query($query);
                if ($resId->rowCount()) {
                    $row = $resId->fetch();
                    $hid = $row['hid'];
                }
            } else {
                 $row = $res->fetch();
                 $hid = $row['host_id'];
            }
            if (!isset($hid)) {
                throw new Exception('Host id of BAM Module could not be found');
            }
            return $hid;
        } catch (Exception $e) {
            echo $e->getMessage() . "<br/>";
        }
    }
}
