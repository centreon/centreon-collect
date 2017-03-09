<?php
/**
 * Copyright 2016 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

require_once realpath(dirname(__FILE__) . '/../../centreon-pluginpack-manager.conf.php');
require_once _CENTREON_PATH_ . '/www/class/centreonRestHttp.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonMedia.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonHosttemplates.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonServicetemplates.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonCommand.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonTimeperiod.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonLog.class.php';
require_once _CENTREON_PATH_ . "/www/api/class/webService.class.php";
require_once _CENTREON_PATH_ . "/www/api/exceptions.php";
require_once MODULE_PATH . '/core/class/centreonPluginPack.class.php';
require_once MODULE_PATH . '/core/class/License.class.php';
require_once MODULE_PATH . '/core/class/centreonPluginPack/PPLocal.class.php';
require_once MODULE_PATH . '/core/class/PluginPackManagerFactory.php';
require_once MODULE_PATH . '/core/class/ImpApi.class.php';
require_once MODULE_PATH . '/core/class/System.class.php';

/**
 * Class for endpoints of centreon license manager
 * @author Centreon
 * @version 1.0.0
 * @package centreon-pluginpack-manager
 */
class CentreonPpManagerPluginpack extends CentreonWebService
{
    /**
     *
     * @var CentreonDB Centreon database connector
     */
    protected $db;

    protected $impApi;

    /**
     * Constructor
     * @param CentreonDB $database
     */
    public function __construct($database = null)
    {
        if (is_null($database)) {
            $this->db = new CentreonDB();
        }
        $pluginPackManagerFactory = new \PluginPackManagerFactory(new CentreonDB());
        $this->impApi = $pluginPackManagerFactory->newImpApi(CENTREON_IMP_API_URL);
        parent::__construct();
    }

    /**
     * Get the list of pluginpack installed
     * @return array
     */
    public function getListInstalledOrdered()
    {
        $aFilters = array();

        if (!empty($this->arguments['locale'])) {
            $locale = $this->arguments['locale'];
        } else {
            $locale = 'en';
        }

        if (!empty($this->arguments['name'])) {
            $name = $this->arguments['name'];
        } else {
            $name = '';
        }

        if (!empty($this->arguments['category'])) {
            $category = $this->arguments['category'];
        } else {
            $category = '';
        }

        if (!empty($this->arguments['status']) && $this->arguments['status'] != 'all') {
            $status = $this->arguments['status'];
        } else {
            $status = '';
        }

        if (!empty($this->arguments['lastUpdate'])) {
            $lastUpdate = $this->arguments['lastUpdate'];
        } else {
            $lastUpdate = '';
        }

        if (!empty($this->arguments['operator'])) {
            $operator = $this->arguments['operator'];
        } else {
            $operator = '';
        }

        $aFilters['search'] = $name;
        $aFilters['category'] = $category;
        $aFilters['status'] = $status;
        $aFilters['lastUpdate'] = $lastUpdate;
        $aFilters['operator'] = $operator;

        $pluginpackConnector = CentreonPluginPack::factory($this->db);
        $data = $pluginpackConnector->getListInstalledOrdered($aFilters);

        $defaultIconPath = realpath(dirname(__FILE__) . '/../../static/img/pp-logo.png');
        $defaultIcon = base64_encode(file_get_contents($defaultIconPath));

        if (!isset($_SESSION['centreon'])) {
            CentreonSession::start();
        }

        if (isset($_SESSION['centreon'])) {
            $centreon = $_SESSION['centreon'];
        } else {
            exit;
        }

        $localeShortName = $centreon->user->get_lang();
        putenv("LANG=$localeShortName");
        setlocale(LC_ALL, $localeShortName);
        bindtextdomain("messages", MODULE_PATH . "locale/");
        bind_textdomain_codeset("messages", "UTF-8");
        textdomain("messages");

        foreach ($data as &$pluginPack) {
            if (!isset($pluginPack['icon_file']) || is_null($pluginPack['icon_file'])) {
                $pluginPack['icon_file'] = $defaultIcon;
            }

            if (isset($pluginPack['description']) && is_array($pluginPack['description'])) {
                $pluginPack['description'] = $this->filterDescription($pluginPack['description'], $locale);
            }

            if (isset($pluginPack['status'])) {
                $pluginPack['status_badge'] = $this->getStatusBadge($pluginPack['status']);
                $pluginPack['label_status'] = _(ucfirst($pluginPack['status']));
            }
        }

        return array(
            'data' => $data
        );
    }


    /**
     * Get the list of pluginpack
     * @return array
     */
    public function getList()
    {
        $aFilters = array();
        if (!empty($this->arguments['offset'])) {
            $offset = $this->arguments['offset'];
        } else {
            $offset = 0;
        }

        if (!empty($this->arguments['limit'])) {
            $limit = $this->arguments['limit'];
        } else {
            $limit = 20;
        }

        if (!empty($this->arguments['locale'])) {
            $locale = $this->arguments['locale'];
        } else {
            $locale = 'en';
        }

        if (!empty($this->arguments['name'])) {
            $name = $this->arguments['name'];
        } else {
            $name = '';
        }
        if (!empty($this->arguments['category'])) {
            $category = $this->arguments['category'];
        } else {
            $category = '';
        }
        if (!empty($this->arguments['status']) && $this->arguments['status'] != 'all') {
            $status = $this->arguments['status'];
        } else {
            $status = '';
        }

        if (!empty($this->arguments['lastUpdate'])) {
            $lastUpdate = $this->arguments['lastUpdate'];
        } else {
            $lastUpdate = '';
        }

        if (!empty($this->arguments['operator'])) {
            $operator = $this->arguments['operator'];
        } else {
            $operator = '';
        }

        $name = trim($name);
        $aFilters['search'] = preg_replace('/(\s+)/', ',', $name);
        $aFilters['category'] = $category;
        $aFilters['status'] = $status;
        $aFilters['lastUpdate'] = $lastUpdate;
        $aFilters['operator'] = $operator;

        $pluginpackConnector = CentreonPluginPack::factory($this->db);
        $data = $pluginpackConnector->getList($offset, $limit, $aFilters);

        $defaultIconPath = realpath(dirname(__FILE__) . '/../../static/img/pp-logo.png');
        $defaultIcon = base64_encode(file_get_contents($defaultIconPath));

        if (!isset($_SESSION['centreon'])) {
            CentreonSession::start();
        }

        if (isset($_SESSION['centreon'])) {
            $centreon = $_SESSION['centreon'];
        } else {
            exit;
        }

        $localeShortName = $centreon->user->get_lang();
        putenv("LANG=$localeShortName");
        setlocale(LC_ALL, $localeShortName);
        bindtextdomain("messages", MODULE_PATH . "locale/");
        bind_textdomain_codeset("messages", "UTF-8");
        textdomain("messages");

        if (is_array($data)) {
            foreach ($data as &$pluginPack) {
                if (!isset($pluginPack['icon_file']) || is_null($pluginPack['icon_file'])) {
                    $pluginPack['icon_file'] = $defaultIcon;
                }

                if (isset($pluginPack['description']) && is_array($pluginPack['description'])) {
                    $pluginPack['description'] = $this->filterDescription($pluginPack['description'], $locale);
                }

                if (isset($pluginPack['status'])) {
                    $pluginPack['status_badge'] = $this->getStatusBadge($pluginPack['status']);
                    $pluginPack['label_status'] = _(ucfirst($pluginPack['status']));
                }
            }
        }

        return array(
            'data' => $data
        );
    }


    /**
     * Set Plugin pack status color
     * @param string $status
     * @return string
     */
    public function getStatusBadge($status)
    {
        $statusColor = "";

        switch ($status) {
            default:
            case "development":
                $statusColor .= "unknown";
                break;
            case "stable":
                $statusColor .= "default_badge";
                break;
            case "deprecated":
                $statusColor .= "danger";
                break;
            case "experimental":
                $statusColor .= "warning";
                break;
        }

        return $statusColor;
    }

    /**
     * Filter description of plugin packs
     * @param array $availableDescription
     * @param string $locale
     * @return string
     */
    public function filterDescription($availableDescription, $locale)
    {
        $finalDescription = '';

        foreach ($availableDescription as $pluginPackDescription) {
            if ($pluginPackDescription['lang'] === $locale) {
                $finalDescription = htmlspecialchars(stripcslashes($pluginPackDescription['value']));
                break;
            }
        }

        return $finalDescription;
    }

    /**
     * Get one plugin pack
     * @return array
     */
    public function get()
    {
        // Init filter array
        $aFilters = array();

        // Get name of the plugin pack
        if (!empty($this->arguments['name'])) {
            $name = $this->arguments['name'];
        } else {
            $name = '';
        }
        $aFilters['name'] = $name;

        // Get plugin pack information
        $pluginpackConnector = CentreonPluginPack::factory($this->db);
        $data = $pluginpackConnector->getList(0, 1, $aFilters);
        return array(
            'data' => $data
        );
    }

    /**
     *
     * @return array
     * @throws Exception
     */
    private function parseInstalUpdateRemoveParams()
    {
        $myArguments = array();

        if (!empty($this->arguments['name'])) {
            $myArguments['name'] = $this->arguments['name'];
        } else {
            throw new Exception("The name must be defined");
        }

        if (!empty($this->arguments['slug'])) {
            $myArguments['slug'] = $this->arguments['slug'];
        } else {
            throw new Exception("The slug must be defined");
        }

        if (!empty($this->arguments['version'])) {
            $myArguments['version'] = $this->arguments['version'];
        } else {
            throw new Exception("The version must be defined");
        }

        if (!empty($this->arguments['id'])) {
            $myArguments['id'] = $this->arguments['id'];
        }

        return $myArguments;
    }

    /**
     * Do install/update plugin pack(s)
     *
     * HTTP method for call this function : POST
     *
     * @throws Exception
     * @return array
     *      'installed' : list of plugin packs (infos) installed,
     *      'updated' : list of plugins pack (infos) updated
     *      'installedOrUpdate' : list of plugin packs (infos) installed or updated
     *      'failed'  : plugin pack (info) failed to install
     */
    public function postInstallupdate()
    {
        if (empty($this->arguments['pluginpack'])) {
            throw new Exception("The pluginpack must be defined");
        }

        $installObj = new \CentreonPluginPackManager\Installation\Install($this->impApi);
        $updateObj = new \CentreonPluginPackManager\Installation\Update($this->impApi);

        //Init
        $arr = array(
            'installed' => array(),
            'updated' => array(),
            'failed' => array(),
        );

        // List of plugin pack(s) to install or update
        $pluginPackInfos = $this->arguments['pluginpack'];

        foreach ($pluginPackInfos as $pluginPackInfo) {
            if ($pluginPackInfo['action'] == 'install') {
                // try to install the plugin pack (with transaction)
                $aInstallResult = $installObj->launchOperation(array($pluginPackInfo));

                // If install failed, stop install and update the other plugins packs
                if (!empty($aInstallResult['failed'])) {
                    $arr['failed'] = $aInstallResult['failed'];
                    break;
                }

                $pluginPackInfo['unmanagedObjects'] = $aInstallResult['unmanagedObjects'];
                $pluginPackInfo['managedObjects'] = $aInstallResult['managedObjects'];

                // add plugin pack in list of installed plugin pack
                $arr['installed'][] = $pluginPackInfo;

            } elseif ($pluginPackInfo['action'] == 'update') {
                // run update the plugin pack
                $aUpdateResult = $updateObj->launchOperation(array($pluginPackInfo));
                
                // If install failed, stop install and update the other plugins packs
                if (!empty($aUpdateResult['failed'])) {
                    $arr['failed'] = $aUpdateResult['failed'];
                    break;
                }

                $pluginPackInfo['unmanagedObjects'] = $aUpdateResult['unmanagedObjects'];
                $pluginPackInfo['managedObjects'] = $aUpdateResult['managedObjects'];

                // add plugin pack in list of updated plugin pack
                $arr['updated'][] = $pluginPackInfo;

            }
        }

        return $arr;
    }

    public function postReinstall()
    {
        if (empty($this->arguments['pluginpack'])) {
            throw new Exception("The pluginpack must be defined");
        }

        $updateObj = new \CentreonPluginPackManager\Installation\Update($this->impApi);

        // List of plugin pack(s) to reinstall
        $pluginPackInfos = $this->arguments['pluginpack'];

        $arr = array();
        foreach ($pluginPackInfos as $pluginPackInfo) {
            $aUpdateResult = $updateObj->launchOperation(array($pluginPackInfo));
            
            $pluginPackInfo['unmanagedObjects'] = $aUpdateResult['unmanagedObjects'];
            $pluginPackInfo['managedObjects'] = $aUpdateResult['managedObjects'];

            // add plugin pack in list of updated plugin pack
            $arr['updated'][] = $pluginPackInfo;
        }

        return $arr;
    }

    /**
     *
     * @return type
     */
    public function postCheckUsed()
    {
        $pluginPackInfos = $this->parseInstalUpdateRemoveParams();
        $uninstallObj = new CentreonPluginPackManager\Installation\Uninstall($this->impApi);
        $used = $uninstallObj->checkUsed($pluginPackInfos);
        return $used;
    }

    /**
     *
     * @return type
     * @throws Exception
     */
    public function postRemove()
    {
        if (!empty($this->arguments['pluginpack'])) {
            $pluginPackInfos = $this->arguments['pluginpack'];
        } else {
            throw new Exception("The pluginpack must be defined");
        }

        $removeObj = new \CentreonPluginPackManager\Installation\Uninstall($this->impApi);
        return $removeObj->launchOperation($pluginPackInfos);
    }

    /**
     *
     * @return array
     */
    public function postCheckDependency()
    {
        // Instantiate PP provider.
        $provider = CentreonPluginPackManager\Installation\Provider\ProviderFactory::newProvider($this->impApi);

        // Get the sorted dependency list.
        $pluginPackInfos = $this->parseInstalUpdateRemoveParams();

        $oDependency = new CentreonPluginPackManager\Installation\Utils\DependencySorter($provider);
        $sortedDeps = $oDependency->getSortedDependencies($pluginPackInfos);

        // Determine if dependency is installed or not, in good version or not.
        $finalDeps = array();
        foreach ($sortedDeps as &$dependency) {
            $localPP = new CentreonPluginPackManager\PPLocal($dependency['slug'], new CentreonDb());
            if (!$localPP->isInstalled()) {
                $dependency['status'] = 'toInstall';
                $finalDeps[] = $dependency;
            } else {
                $properties = $localPP->getProperties();
                if (
                    (version_compare($dependency['version'], $properties['version']) > 0) ||
                    ($dependency['complete'] == 0)
                ) {
                    $dependency['status'] = 'toUpgrade';
                    $finalDeps[] = $dependency;
                }
            }
        }
        return $finalDeps;
    }
}
