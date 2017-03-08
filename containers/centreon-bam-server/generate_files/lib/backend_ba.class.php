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

class BackendBa extends AbstractObject {

    public function getBaHostId() {
        try {
            $host_name = '_Module_BAM_' . $this->backend_instance->getPollerId();
            $stmt = $this->backend_instance->db->prepare("SELECT
                  host_id
                FROM host
                WHERE host_register = '2'
                AND host_name = :host_name");
            $stmt->bindParam(':host_name', $host_name, PDO::PARAM_STR);
            $stmt->execute();
            if ($stmt->rowCount()) {
                $row = $stmt->fetch(PDO::FETCH_ASSOC);
                return $row['host_id'];
            }
            $stmt2 = $this->backend_instance->db->prepare("INSERT INTO
                  host (host_name, host_register)
                VALUES (:host_name, '2')");
            $stmt2->bindParam(':host_name', $host_name, PDO::PARAM_STR);
            $stmt2->execute();
            return $this->backend_instance->db->lastInsertId();
        } catch (Exception $e) {
            echo $e->getMessage() . "<br/>";
        }
    }

     public function getBaServiceId($ba_id) {
        try {
            $host_id = $this->getBaHostId();
            $ba_name = 'ba_' . $ba_id;
            $stmt = $this->backend_instance->db->prepare("SELECT
                  service_id
                FROM service
                WHERE service_register = '2'
                AND service_description = :service_description
                ");
            $stmt->bindParam(':service_description', $ba_name, PDO::PARAM_STR);
            $stmt->execute();
            
            $service_id = null;
            if (!$stmt->rowCount()) {
                $stmt2 = $this->backend_instance->db->prepare("INSERT INTO
                    service (service_description, service_register)
                    VALUES (:ba_name, '2')
                    ");
                $stmt2->bindParam(':ba_name', $ba_name, PDO::PARAM_STR);
                $stmt2->execute();
                $service_id = $this->backend_instance->db->lastInsertId();
            } else {
                 $row = $stmt->fetch(PDO::FETCH_ASSOC);
                 $service_id = $row['service_id'];
            }
            $stmt3 = $this->backend_instance->db->prepare("INSERT IGNORE INTO
                host_service_relation (host_host_id, service_service_id)
                VALUES (:host_id, :service_id)
                ");
            $stmt3->bindParam(':host_id', $host_id, PDO::PARAM_INT);
            $stmt3->bindParam(':service_id', $service_id, PDO::PARAM_INT);
            $stmt3->execute();
            return $service_id;
        } catch (Exception $e) {
            echo $e->getMessage() . "<br/>";
        }
    }
    
    public function isBaService($serviceDescription)
    {
        $isBaService = false;
        
        $baId = substr($serviceDescription, 3);
        
        if (is_numeric($baId)) {
            $stmt = $this->backend_instance->db->prepare("SELECT name FROM mod_bam WHERE ba_id = :ba_id");
            $stmt->bindParam(':ba_id', $baId, PDO::PARAM_INT);
            $stmt->execute();

            if ($stmt->rowCount()) {
                $isBaService = true;
            }
        }
        
        return $isBaService;
    }

    public function getBas() {
        try {
            $poller_id = $this->backend_instance->getPollerId();

            $host_name = '_Module_BAM_' . $this->backend_instance->getPollerId();
            $stmt = $this->backend_instance->db->prepare("SELECT
                  b.ba_id, b.name, b.icon_id, b.notification_interval, b.notification_options, 
                  b.notifications_enabled, b.id_notification_period,
                  b.event_handler_enabled, b.event_handler_command, b.event_handler_args
                FROM mod_bam b, mod_bam_poller_relations bap
                WHERE b.activate = '1'
                AND b.ba_id = bap.ba_id
                AND bap.poller_id = :poller_id");
            $stmt->bindParam(':poller_id', $poller_id, PDO::PARAM_INT);
            $stmt->execute();
            return $stmt->fetchAll(PDO::FETCH_ASSOC|PDO::FETCH_GROUP|PDO::FETCH_UNIQUE);
        } catch (Exception $e) {
            echo $e->getMessage() . "<br/>";
        }
    }

    public function hasBa() {
        $stmt = $this->backend_instance->db->prepare("SELECT
              ba_id
            FROM mod_bam_poller_relations
            WHERE poller_id = :poller_id
            ");
        $stmt->bindParam(':poller_id', $this->backend_instance->getPollerId(), PDO::PARAM_STR);
        $stmt->execute();
        if ($stmt->rowCount()) {
            return 1;
        }
        return 0;
    }
}
