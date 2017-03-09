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

class BaContact extends Contact {
    protected $generate_filename = 'centreon-bam-contacts.cfg';
    protected $notif_command;

    public function generateFromContactId($contact_id) {
        if (is_null($contact_id)) {
            return null;
        }

        $this->buildCache();

        if ($this->use_cache == 1) {
            if (!isset($this->contacts_cache[$contact_id])) {
                return null;
            }
            $this->contacts[$contact_id] = &$this->contacts_cache[$contact_id];
        } else if (!isset($this->contacts[$contact_id])) {
            $this->getContactFromId($contact_id);
        }

        if (is_null($this->contacts[$contact_id])) {
            return null;
        }

        if ($this->checkGenerate($contact_id)) {
            return $this->contacts[$contact_id]['register'] == 1 ? $this->contacts[$contact_id]['contact_name'] : $this->contacts[$contact_id]['name'];
        }

        if ($this->contacts[$contact_id]['register'] == 0 && !isset($this->contacts[$contact_id]['name'])) {
            $this->contacts[$contact_id]['name'] = '_Module_BAM_' . $this->contacts[$contact_id]['contact_name'];
            unset($this->contacts[$contact_id]['contact_name']);
        } else if ($this->contacts[$contact_id]['register'] == 1) {
            $this->contacts[$contact_id]['contact_name'] = '_Module_BAM_' . $this->contacts[$contact_id]['contact_name'];
        }

        $this->contacts[$contact_id]['use'] = array();
        $this->contacts[$contact_id]['use'][] = $this->generateFromContactId($this->contacts[$contact_id]['contact_template_id']);

        $this->contacts[$contact_id]['host_notifications_enabled'] = 0;

        /* 
         * Get notifications commands
         */
        if (empty($this->notif_command)) {
            $this->getBAMNotificationCommand();
        }
        $this->contacts[$contact_id]['host_notification_commands'] = array($this->notif_command);
        $this->contacts[$contact_id]['service_notification_commands'] = array($this->notif_command);

        $period = Timeperiod::getInstance();
        $this->contacts[$contact_id]['host_notification_period'] = $period->generateFromTimeperiodId($this->contacts[$contact_id]['host_notification_period_id']);
        $this->contacts[$contact_id]['service_notification_period'] = $period->generateFromTimeperiodId($this->contacts[$contact_id]['service_notification_period_id']);

        $this->generateObjectInFile($this->contacts[$contact_id], $contact_id);
        return $this->contacts[$contact_id]['register'] == 1 ? $this->contacts[$contact_id]['contact_name'] : $this->contacts[$contact_id]['name'];
    }

    /*
     * Get notification command name
     */
    private function getBAMNotificationCommand()
    {
        $this->notif_command = "bam-notify-by-email";

        $stmt = $this->backend_instance->db->prepare("SELECT pref_value FROM mod_bam_user_preferences WHERE pref_key = 'command_id' AND user_id IS NULL");
        $stmt->execute();
        if ($stmt->rowCount()) {
            $row = $stmt->fetch(PDO::FETCH_ASSOC);
            if (!empty($row["pref_value"])) {
                $command = BaCommand::getInstance();
                $this->notif_command = $command->getCommandName($row["pref_value"]);
            }
        }
    }

    public function getGenerateFile()
    {
        return $this->generate_filename;
    }
}

