<?php
/**
 * CENTREON
 *
 * Source Copyright 2005-2016 CENTREON
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@centreon.com
 *
 */
namespace CentreonPluginPackManager\Installation\Object;

/**
 *
 * @author lionel
 */
class Icon extends ObjectManager
{
    /**
     *
     * @var array
     */
    private $images;

    /**
     *
     * @var int
     */
    private $mediaDirectoryId;

    /**
     *
     */
    public function __construct($database)
    {
        parent::__construct($database);
        $this->keyInJson = 'images';
        $this->neededUtils = array('mediaManager');
    }

    /**
     *
     * @return int
     */
    public function getMediaDirectoryId()
    {
        return $this->mediaDirectoryId;
    }

    /**
     *
     * @param int $newMediaDirectoryId
     */
    public function setMediaDirectory($newMediaDirectoryId)
    {
        $this->mediaDirectoryId = $newMediaDirectoryId;
    }

    /**
     *
     * @return array
     */
    public function getObjectParams()
    {
        return $this->images;
    }

    /**
     *
     */
    public function setObjectParams($newObjectParams)
    {
        $this->images = $newObjectParams;
    }

    /**
     * Prepare install
     */
    public function prepareInstall()
    {

    }

    /**
     * Prepare update
     */
    public function prepareUpdate()
    {

    }

    /**
     * Prepare uninstall
     */
    public function prepareUninstall()
    {

    }

    /**
     *
     * @param string $icon
     * @return int
     */
    public function getIcon($icon)
    {
        $iconId = $this->getIconId($icon);
        if (is_null($iconId)) {
            $iconId = $this->addIcon($icon);
        }

        return $iconId;
    }

    /**
     *
     * @param string $icon
     * @return int
     * @throws Exception
     */
    private function addIcon($icon)
    {
        $queryInsertIcon = "INSERT INTO mod_ppm_icons(`icon_file`) VALUES('$icon')";
        $res = $this->dbManager->query($queryInsertIcon);
        if (\PEAR::isError($res)) {
            throw new \Exception('Error while adding icon');
        }

        $iconId = $this->getIconId($icon);

        return $iconId;
    }

    /**
     *
     * @param string $icon
     * @return mixed
     * @throws Exception
     */
    private function getIconId($icon)
    {
        $iconId = null;

        $querySelectIcon = "SELECT `icon_id` FROM `mod_ppm_icons` WHERE `icon_file` = '$icon'";
        $res = $this->dbManager->query($querySelectIcon);
        if (\PEAR::isError($res)) {
            throw new \Exception('Error while getting icon');
        }

        $row = $res->fetchRow();
        if ($row !== false) {
            $iconId = $row['icon_id'];
        }

        return $iconId;
    }

    /**
     * Install icons
     */
    public function launchInstall()
    {
        $this->icons = array();
        if (!isset($this->images)) {
            return $this->icons;
        }

        foreach ($this->images as $image) {
            $mediaId = $this->insertMedia($image['name'], $image['icon']);
            $this->icons[$image['name']] = array(
                'mediaId' => $mediaId,
                'icon' => $image['icon']
            );
        }
    }

    /**
     * Update icons
     */
    public function launchUpdate()
    {
        $this->launchInstall();
    }

    /**
     * Remove icons
     */
    public function launchUninstall()
    {

    }

    /**
     *
     * @param string $icon
     * @return int
     * @throws Exception
     */
    private function insertMedia($iconName, $iconBinary)
    {
        $mediaDirectoryName = 'ppm';
        $iconParameters = array(
            'img_path' => $iconName,
            'img_name' => $iconName,
            'dir_name' => $mediaDirectoryName
        );

        $this->utilsManager['mediaManager']->setMediaDirectory(_CENTREON_PATH_ . '/www/img/media/');
        $mediaId = $this->utilsManager['mediaManager']->addImage($iconParameters, $iconBinary);

        return $mediaId;
    }
}
