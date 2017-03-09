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

class BaService extends AbstractObject {
    protected $generate_filename = 'centreon-bam-services.cfg';
    protected $object_name = 'service';
    protected $attributes_write = array(
        'service_description',
        'display_name',
        'host_name',
        'check_command',
        'max_check_attempts',
        'normal_check_interval',
        'retry_check_interval',
        'active_checks_enabled',
        'passive_checks_enabled',
        'check_period',
        'notification_interval',
        'notification_period',
        'notification_options',
        'notifications_enabled',
        'event_handler_enabled',
        'event_handler',
        'contact_groups',
        '_SERVICE_ID',
        'register'
    );
    protected $ba_poller = null;
    protected $ba_host_id = null;
    protected $contactgroups_from_id = array();
    protected $contactgroups = array();

    public function generateObjects($localhost) {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        Engine::getInstance()->addCfgPath($this->generate_filename);

        if (is_null($this->ba_host_id)) {
            $this->ba_host_id = BackendBA::getInstance()->getBaHostId();
        }

        $poller_id = Backend::getInstance()->getPollerId();
        $bas = BackendBA::getInstance()->getBas();
        foreach ($bas as  $ba_id => $parameters) {
            $object = array();
            $service_id = BackendBA::getInstance()->getBaServiceId($ba_id);

            /* 
             * Set BA Name : useful for the notification
             */ 
            $object['display_name'] = $parameters['name'];

            $object['host_name'] = '_Module_BAM_' . $poller_id;
            $object['service_description'] = 'ba_' .  $ba_id;
            $object['max_check_attempts'] = '1';
            $object['normal_check_interval'] = '1';
            $object['retry_check_interval'] = '1';
            $object['check_command'] = 'centreon-bam-check!' . $ba_id;
            $object['check_period'] = 'centreon-bam-timeperiod';
            $object['active_checks_enabled'] = '1';
            $object['passive_checks_enabled'] = '1';

            if (isset($parameters['icon_id'])) {
                $object['icon_image'] = Media::getInstance()->getMediaPathFromId($parameters['icon_id']);
            }

            /*
             * Enable notifications only on central
             */
            $object['notifications_enabled'] = $this->getNotificationEnable($ba_id, $parameters['notifications_enabled'], $localhost);
            
            $contactgroups = $this->getContactgroupsFromId($ba_id);
            if (count($contactgroups)) {
                $object['contact_groups'] = implode(',', array_values($contactgroups));
            }
            $this->getContactgroupsFromEscalations($ba_id);
            
            $object['notification_period'] = Timeperiod::getInstance()->generateFromTimeperiodId($parameters['id_notification_period']);
            $object['notification_interval'] = $parameters['notification_interval'];
            $object['notification_options'] = $parameters['notification_options'];

            /*
             * Event Handler
             */
            $object['event_handler_enabled'] = $parameters['event_handler_enabled'];
            if ($parameters['event_handler_enabled']) {
                $command_name = $this->getEventHandler($parameters['event_handler_command']);
                if ($command_name != "") {
                    $object['event_handler'] = $command_name.$parameters['event_handler_args'];                    
                }
            }

            /* 
             * Macro 
             */
            $object['_SERVICE_ID'] = $service_id;
            $object['_SERVICE_BAID'] = $ba_id;

            $object['register'] = '1';

            /* 
             * For index data
             */
            Service::getInstance()->addGeneratedServices($this->ba_host_id, $service_id);
            Service::getInstance()->addServiceCache($service_id, $object);
            $this->generateObjectInFile($object, 0);
        }

    }

    private function getEventHandler($event_handler_command_id) {
        $stmt = $this->backend_instance->db->prepare("SELECT command_name FROM command WHERE command_id = :cmd_id LIMIT 1");
        $stmt->bindParam(':cmd_id', $event_handler_command_id, PDO::PARAM_INT);
        $stmt->execute();
        $command = $stmt->fetch(PDO::FETCH_ASSOC);
        if (isset($command) && $command["command_name"]) {
            return $command['command_name'];
        } else {
            return "";
        }
    }

    private function getNotificationEnable($ba_id, $notificationsEnabled, $localhost) {
        if (!$localhost) {
            return $notificationsEnabled;
        }
        if (!isset($this->ba_poller)) {
            if (isset($this->ba_poller[$ba_id]) && $this->ba_poller[$ba_id]['count_ba'] > 1) {
                return '0';
            } else {
                return $notificationsEnabled;
            }
        }
        
        $this->ba_poller = array();
        $stmt = $this->backend_instance->db->prepare("SELECT ba_id, count(ba_id) as count_ba FROM mod_bam_poller_relations");
        $stmt->execute();
        $this->ba_poller = $stmt->fetchAll(PDO::FETCH_GROUP|PDO::FETCH_UNIQUE|PDO::FETCH_ASSOC);

        if (!isset($this->ba_poller) && isset($this->ba_poller[$ba_id]) && $this->ba_poller[$ba_id]['count_ba'] > 1) {
            return '0';
        }
        return $notificationsEnabled;
    }

    private function getContactgroupsFromId($ba_id) {
        if (isset($this->contactgroups_from_id[$ba_id])) {
            return $this->contactgroups_from_id[$ba_id];
        }

        $contactgroups = array();
        $stmt = $this->backend_instance->db->prepare("SELECT
              cg.cg_id, cg.cg_name
            FROM mod_bam_cg_relation ocgr, contactgroup cg
            WHERE ocgr.id_ba = :ba_id
            AND ocgr.id_cg = cg.cg_id
            ORDER BY cg_name
            ");
        $stmt->bindParam(':ba_id', $ba_id, PDO::PARAM_INT);
        $stmt->execute();

        if ($stmt->rowCount() == 0) {
            return null;
        }

        while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
            $contactgroup = '_Module_BAM_' . html_entity_decode($row["cg_name"], ENT_QUOTES, "UTF-8");
            $this->contactgroups[$row["cg_id"]] = $contactgroup;
            $contactgroups[$row["cg_id"]] = $contactgroup;
        }

        $this->contactgroups = $this->contactgroups + $contactgroups;
        $this->contactgroups_from_id[$ba_id] = $contactgroups;
        return $contactgroups;
    }
    
    private function getContactgroupsFromEscalations($ba_id)
    {
        $contactgroups = array();
        $queryGetEscalations = "SELECT id_esc FROM mod_bam_escal_relation WHERE id_ba = :ba_id";
        $queryGetCg = "SELECT cg.cg_id, cg.cg_name "
            . "FROM escalation_contactgroup_relation ecr, contactgroup cg "
            . "WHERE escalation_esc_id = :esc_id "
            . "AND ecr.contactgroup_cg_id = cg.cg_id";
        
        
        $stmtEscalations = $this->backend_instance->db->prepare($queryGetEscalations);
        $stmtEscalations->bindParam(':ba_id', $ba_id, PDO::PARAM_INT);
        $stmtEscalations->execute();
        
        if ($stmtEscalations->rowCount() == 0) {
            return null;
        }
        
        while ($rowEsc = $stmtEscalations->fetch(PDO::FETCH_ASSOC)) {
            $stmtCgs = $this->backend_instance->db->prepare($queryGetCg);
            $stmtCgs->bindParam(':esc_id', $rowEsc["id_esc"], PDO::PARAM_INT);
            $stmtCgs->execute();
            
            while ($rowCg = $stmtCgs->fetch(PDO::FETCH_ASSOC)) {
                $contactgroup = '_Module_BAM_' . html_entity_decode($rowCg["cg_name"], ENT_QUOTES, "UTF-8");
                $this->contactgroups[$rowCg["cg_id"]] = $contactgroup;
                $contactgroups[$rowCg["cg_id"]] = $contactgroup;
            }
        }

        $this->contactgroups = $this->contactgroups + $contactgroups;
        $this->contactgroups_from_id[$ba_id] = $contactgroups;
        return $contactgroups;
    }

    public function getContactgoups() {
        return $this->contactgroups;
    }

    public function reset() {
        $this->ba_host_id = null;
        $this->ba_services_ids = null;
        parent::reset();
    }
}
