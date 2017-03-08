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

class BaCommand extends AbstractObject {
    protected $generate_filename = 'centreon-bam-command.cfg';
    protected $object_name = 'command';
    protected $attributes_write = array(
        'command_name',
        'command_line',
    );
    protected $notification_command = null;
    protected $service_check_command = null;
    protected $service_event_handler_command = null;
    protected $host_check_command = null;
    protected $plugins_path = null;
    protected $mail_bin = null;

    public function generateObjects() {
        if ($this->checkGenerate(0)) {
            return 0;
        }

        if (is_null($this->plugins_path)) {
            $this->getPluginsPath();
        }

        if (is_null($this->mail_bin)) {
            $this->getMailBin();
        }

        Engine::getInstance()->addCfgPath($this->generate_filename);

        $commands = array();

        /* Notification command */
        $notificationCommand = $this->getNotificationCommand();
        if (!is_null($notificationCommand)) {
            $commands[] = $notificationCommand;
        }

        $commands[] = $this->getServiceCheckCommand();
        $commands[] = $this->getHostCheckCommand();

	/* Event Handler Commands */
	$eventHandlerCommand = $this->getServiceEventHandlerCommand(); 
        foreach ($eventHandlerCommand as $ehc) {
            $commands[] = $ehc;
        }    

        foreach ($commands as $command) {
            $object = array();
            $object['command_name'] = $command['command_name'];
            $object['command_line'] = str_replace("@MAILER@", $this->mail_bin, $command['command_line']);
            if ($command['enable_shell']) {
                $object['command_line'] = '/bin/sh -c ' . escapeshellarg($object['command_line']);
            }

            $this->generateObjectInFile($object, 0);
        }
    }

    private function getMailBin() {
        $stmt = $this->backend_instance->db->prepare("SELECT 
              options.value
            FROM options
                WHERE options.key = 'mailer_path_bin'
            ");
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
            $this->mail_bin = $row['value'];
        } else {
            $this->mail_bin = '';
        }
    }

    private function getNotificationCommand() {
        if (!is_null($this->notification_command)) {
            return $this->notification_command;
        }

        $stmt = $this->backend_instance->db->prepare("SELECT c.command_id, c.command_name, c.command_line, c.enable_shell
                                                    FROM mod_bam_user_preferences pref
                                                    INNER JOIN command c on CAST(pref.pref_value AS SIGNED) = c.command_id 
                                                    AND pref.pref_key = 'command_id' 
                                                    AND c.command_type = 1
        ");
        $stmt->execute();
        if ($stmt->rowCount()) {
            $data = $stmt->fetch(PDO::FETCH_ASSOC);
            if (Command::getInstance()->checkGenerate($data['command_id'])) {
                return null;
            }
            unset($data['command_id']);
            $data['command_line'] = str_replace("@MAILER@", $this->mail_bin, $data['command_line']);
            $this->notification_command = $data;
        } else {
            $this->notification_command = array(
                'command_name' => 'bam-notify-by-email',
                'command_line' => str_replace("@MAILER@", $this->mail_bin, '/usr/bin/printf \"%b\" \"***** Centreon BAM *****\\n\\n'
                    . 'Notification Type: \$NOTIFICATIONTYPE\$\\n\\nBusiness Activity: \$SERVICEDISPLAYNAME\$\\n'
                    . 'State: \$SERVICESTATE\$\\n\\nDate: \$DATE\$ \$TIME\$\\n\\nAdditional Info:\\n\\n\$SERVICEOUTPUT\$\" '
                    . '| @MAILER@ -s \"** \$NOTIFICATIONTYPE\$ - \$SERVICEDISPLAYNAME\$ is \$SERVICESTATE\$ **\" \$CONTACTEMAIL\$'),
                'enable_shell' => 0
            );
        }

        return $this->notification_command;
    }

    private function getPluginsPath() {
        $stmt = $this->backend_instance->db->prepare("SELECT
          options.value
        FROM options
        WHERE options.key = 'nagios_path_plugins'
        ");

        $stmt->execute();

        if ($stmt->rowCount()) {
            $row = $stmt->fetch(PDO::FETCH_ASSOC);
            $this->plugins_path = $row['value'];
        } else {
            $this->plugins_path = '$USER1$';
        }
    }

    private function getServiceCheckCommand() {
        if (!is_null($this->service_check_command)) {
            return $this->service_check_command;
        }

        $this->service_check_command = array(
            "command_name" => "centreon-bam-check",
            "command_line" => $this->plugins_path . '/check_centreon_bam -i $ARG1$',
            "enable_shell" => 0
        );

        return $this->service_check_command;
    }

    private function getHostCheckCommand() {
        if (!is_null($this->host_check_command)) {
            return $this->host_check_command;
        }

        $this->host_check_command = array(
            "command_name" => "centreon-bam-host-alive",
            "command_line" => $this->plugins_path . '/check_ping -H $HOSTADDRESS$ -w 3000.0,80% -c 5000.0,100% -p 1',
            "enable_shell" => 0
        );

        return $this->host_check_command;
    }

    private function getServiceEventHandlerCommand() {
        if (!is_null($this->service_event_handler_command)) {
            return $this->service_event_handler_command;
        }

        $this->service_event_handler_command = array();

        $stmt = $this->backend_instance->db->prepare("SELECT command_name, command_line, enable_shell FROM command WHERE command_type = 3 AND command_id IN (select DISTINCT event_handler_command FROM mod_bam WHERE event_handler_command IS NOT NULL)");
        $stmt->execute();
        if ($stmt->rowCount()) {
            while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
        	    $this->service_event_handler_command[] = $row;
        	}
        }
        return $this->service_event_handler_command;
    }

    public function getCommandName($command_id) {
        if (!isset($command_id)) {
            return null;
        }

        $stmt = $this->backend_instance->db->prepare("SELECT command_name FROM command WHERE command_id = :command_id");
        $stmt->bindParam(':command_id', $command_id, PDO::PARAM_INT);
        $stmt->execute();
        if ($stmt->rowCount() == 0) {
            return -1;
        } else {
            $row = $stmt->fetch(PDO::FETCH_ASSOC);
            if (isset($row['command_name']) && $row['command_name'] != '') {
                return $row['command_name'];
            } else {
                return -2;
            }
        }
    }
}
