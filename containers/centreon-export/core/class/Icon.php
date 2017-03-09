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

class Icon
{

    /**
     *
     * @var int identifiant of icon 
     */
    protected $Icons_id;

    /**
     *
     * @var string Text of icon 
     */
    protected $Icons_name;
    
    protected $sDefaultDirectory;

    
    /**
     *
     * @var string Text of icon 
     */
    protected $Icons_file;
    public $aAuthorizedExtension = array('jpeg', 'png', 'jpg');

    public function __construct()
    {
        $this->db = new DBManager();
        $this->setDefaultDirectory();
    }

    public function uploadIcon($sIcon)
    {
        $id = 0;

        $fileinfo = $sIcon->getValue();

        if (!isset($fileinfo["name"]) | !isset($fileinfo["type"])) {
            return false;
        }

        if ($this->isAuthorizedExtension($fileinfo["type"])) {
            throw new Exception("unknown extension");
        }
        $sFile = $this->convertIcon($fileinfo["tmp_name"]);

        if ($sFile) {
            $id = $this->insertIcon($sFile);
        }
        return $id;
    }

    public function isAuthorizedExtension($sExtension)
    {
        $bReturn = false;
        if (in_array(strtolower($sExtension), $this->aAuthorizedExtension)) {
            $bReturn = true;
        }
        return $bReturn;
    }

    public function insertIcon($sIcon)
    {
        $id = 0;
        if (!empty($sIcon)) {
            $sQuery = "INSERT INTO `mod_export_icons`(`icons_file`) VALUES (:file)";
            $sth = $this->db->db->prepare($sQuery);

            $sth->bindParam(':file', $sIcon, \PDO::PARAM_STR);

            try {
                $sth->execute();
                $id = $this->db->db->lastInsertId();
            } catch (\PDOException $e) {
                echo "Error " . $e->getMessage();
            }
        } else {
            throw new \Exception("empty icon file");
        }
        return $id;
    }

    /**
     * This method delete the icon of the plugin
     * 
     * @param int $iIdPlugin Identifiant of plugin
     */
    public function deleteIconByPlugin($iIdPlugin)
    {
        if (empty($iIdPlugin)) {
            return false;
        }
        $sQuery = 'DELETE `mod_export_icons` FROM `mod_export_icons` JOIN `mod_export_pluginpack` ON `plugin_icon` = `icons_id` WHERE plugin_id = :plugin_id ';
        $sth = $this->db->db->prepare($sQuery);
        $sth->bindParam(':plugin_id', $iIdPlugin, \PDO::PARAM_INT);
        try {
            $sth->execute();
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }
    }

    /**
     * This method convert the file on base64
     * 
     * @param string $sFile
     * @return boolean
     */
    public function convertIcon($sFile)
    {
        if (file_exists($sFile)) {
            return base64_encode(fread(fopen($sFile, "r"), filesize($sFile)));
        } else {
            return false;
        }
    }

    /**
     * This method return the icon
     * 
     * @param int $iIdIcon Identifiant of icon
     * @return mixed (string when icon exist, boolean when icon is empty)
     */
    public function getIcon($iIdIcon)
    {
        $return = false;
        if (empty($iIdIcon)) {
            return $return;
        }

        $sQuery = "SELECT `icons_file` from  `mod_export_icons` WHERE icons_id = :icon_id LIMIT 1";
        $sth = $this->db->db->prepare($sQuery);

        $sth->bindParam(':icon_id', $iIdIcon, \PDO::PARAM_STR);

        try {
            $sth->execute();
            $aData = $sth->fetch(\PDO::FETCH_ASSOC);
            $return = $aData['icons_file'];
        } catch (\PDOException $e) {
            echo "Error " . $e->getMessage();
        }

        return $return;
    }
    
    /**
     * 
     * @return type
     */
    public function setDefaultDirectory()
    {
        $sQuery = "SELECT `value` FROM `options` where `key` = 'nagios_path_img'";
        $res = $this->db->db->query($sQuery);
        $aData = $res->fetch(\PDO::FETCH_ASSOC);

        if (!isset($aData["nagios_path_img"])) {
            $sDefault = _CENTREON_PATH_ . 'www/img/media/';
        } else {
            $sDefault = $aData["nagios_path_img"];
        }

        $this->sDefaultDirectory = $sDefault;
 
    }
    
    /**
     * 
     * @return type
     */
    public function getDefaultDirectory()
    {
        return $this->sDefaultDirectory;
    }
}
