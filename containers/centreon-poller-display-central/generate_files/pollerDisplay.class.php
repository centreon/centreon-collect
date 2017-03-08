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

require_once dirname(__FILE__) . '/../centreon-poller-display-central.conf.php';

use \CentreonPollerDisplayCentral\ConfigGenerate\Bam;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon;

class PollerDisplay extends \AbstractObject
{
    /**
     *
     * @var boolean 
     */
    protected $engine = false;
    
    /**
     *
     * @var boolean 
     */
    protected $broker = true;
    
    /**
     *
     * @var string 
     */
    protected $generate_filename = 'bam-poller-display.sql';

    /**
     * 
     * @param int $poller_id
     * @param string $localhost
     */
    public function generateFromPollerId($poller_id, $localhost)
    {
        $this->poller_id = $poller_id;
        $stmt = $this->backend_instance->db->prepare(
            "SELECT id " .
            "FROM mod_poller_display_server_relations " .
            "WHERE nagios_server_id = :pollerId"
        );
        $stmt->bindParam(':pollerId', $poller_id, PDO::PARAM_INT);
        $stmt->execute();

        if ($stmt->fetch()) {
            Centreon::getInstance()->generateobjects($poller_id);
            $bamGenerateInstance = Bam::getInstance();
            
            if ($bamGenerateInstance->isBamModuleAvailable()) {
                $bamGenerateInstance->generateobjects($poller_id);
            }
        }
    }
}
