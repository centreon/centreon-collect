<?php
/*
 * Copyright 2005-2017 Centreon
 * Centreon is developped by : Julien Mathis and Romain Le Merlus under
 * GPL Licence 2.0.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation ; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses>.
 *
 * Linking this program statically or dynamically with other modules is making a
 * combined work based on this program. Thus, the terms and conditions of the GNU
 * General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this program give Centreon
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of Centreon choice, provided that
 * Centreon also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 */

namespace CentreonPollerDisplayCentral\ConfigGenerate;

use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclActions;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclActionsRules;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclGroupActionsRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclGroupContactgroupsRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclGroupContactsRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclGroupTopology;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclGroups;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResources;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesGroupRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesHost;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesHostCategorie;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesHostex;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesHostgroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesMeta;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesPoller;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesService;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesServiceCategorie;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\AclResourcesServicegroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Contact;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactHostRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactServiceRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Contactgroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactgroupContactRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactgroupHostRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactgroupHostgroupRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactgroupServiceRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ContactgroupServicegroupRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Host;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostCategories;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostCategoriesRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostInformation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostServiceRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Hostgroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\HostgroupRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\MetaContact;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\MetaContactgroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\MetaService;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\MetaServiceRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\NagiosCfg;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\NagiosServer;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Service;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ServiceCategories;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ServiceCategoriesRelation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ServiceInformation;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\Servicegroup;
use \CentreonPollerDisplayCentral\ConfigGenerate\Centreon\ServicegroupRelation;

class Centreon extends \AbstractObject
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
    protected $generate_filename = 'centreon-poller-display.sql';

    /**
     * 
     * @param int $poller_id
     */
    public function generateObjects($poller_id)
    {
        $db = $this->backend_instance->db;

        $oAclActions = new AclActions($db, $poller_id);
        $oAclActionsRules = new AclActionsRules($db, $poller_id);
        $oAclGroupActionsRelation = new AclGroupActionsRelation($db, $poller_id);
        $oAclGroupContactgroupsRelation = new AclGroupContactgroupsRelation($db, $poller_id);
        $oAclGroupContactsRelation = new AclGroupContactsRelation($db, $poller_id);
        $oAclGroupTopology = new AclGroupTopology($db, $poller_id);
        $oAclGroups = new AclGroups($db, $poller_id);
        $oAclResources = new AclResources($db, $poller_id);
        $oAclResourcesGroupRelation = new AclResourcesGroupRelation($db, $poller_id);
        $oAclResourcesHost = new AclResourcesHost($db, $poller_id);
        $oAclResourcesHostCategorie = new AclResourcesHostCategorie($db, $poller_id);
        $oAclResourcesHostex = new AclResourcesHostex($db, $poller_id);
        $oAclResourcesHostgroup = new AclResourcesHostgroup($db, $poller_id);
        $oAclResourcesMeta = new AclResourcesMeta($db, $poller_id);
        $oAclResourcesPoller = new AclResourcesPoller($db, $poller_id);
        $oAclResourcesService = new AclResourcesService($db, $poller_id);
        $oAclResourcesServiceCategorie = new AclResourcesServiceCategorie($db, $poller_id);
        $oAclResourcesServicegroup = new AclResourcesServicegroup($db, $poller_id);
        $oContact = new Contact($db, $poller_id);
        $oContactHostRelation = new ContactHostRelation($db, $poller_id);
        $oContactServiceRelation = new ContactServiceRelation($db, $poller_id);
        $oContactgroup = new Contactgroup($db, $poller_id);
        $oContactgroupContactRelation = new ContactgroupContactRelation($db, $poller_id);
        $oContactgroupHostRelation = new ContactgroupHostRelation($db, $poller_id);
        $oContactgroupHostgroupRelation = new ContactgroupHostgroupRelation($db, $poller_id);
        $oContactgroupServiceRelation = new ContactgroupServiceRelation($db, $poller_id);
        $oContactgroupServicegroupRelation = new ContactgroupServicegroupRelation($db, $poller_id);
        $oHost = new Host($db, $poller_id);
        $oHostCategories = new HostCategories($db, $poller_id);
        $oHostCategoriesRelation = new HostCategoriesRelation($db, $poller_id);
        $oHostInformation = new HostInformation($db, $poller_id);
        $oHostRelation = new HostRelation($db, $poller_id);
        $oHostServiceRelation = new HostServiceRelation($db, $poller_id);
        $oHostgroup = new Hostgroup($db, $poller_id);
        $oHostgroupRelation = new HostgroupRelation($db, $poller_id);
        $oMetaContact = new MetaContact($db, $poller_id);
        $oMetaContactgroup = new MetaContactgroup($db, $poller_id);
        $oMetaService = new MetaService($db, $poller_id);
        $oMetaServiceRelation = new MetaServiceRelation($db, $poller_id);
        $oNagiosCfg = new NagiosCfg($db, $poller_id);
        $oNagiosServer = new NagiosServer($db, $poller_id);
        $oService = new Service($db, $poller_id);
        $oServiceCategories = new ServiceCategories($db, $poller_id);
        $oServiceCategoriesRelation = new ServiceCategoriesRelation($db, $poller_id);
        $oServiceInformation = new ServiceInformation($db, $poller_id);
        $oServicegroup = new Servicegroup($db, $poller_id);
        $oServicegroupRelation = new ServicegroupRelation($db, $poller_id);

        $sql = '';
        $sql .= $this->setForeignKey(0). "\n\n";

        $nagiosServerList = $oNagiosServer->getList();
        $sql .= $oNagiosServer->generateSql($nagiosServerList) . "\n\n";

        $nagiosCfgList = $oNagiosCfg->getList();
        $sql .= $oNagiosCfg->generateSql($nagiosCfgList) . "\n\n";

        $aclResourcesPollerList = $oAclResourcesPoller->getList();
        $sql .= $oAclResourcesPoller->generateSql($aclResourcesPollerList) . "\n\n";

        $hostRelationList = $oHostRelation->getList();
        $sql .= $oHostRelation->generateSql($hostRelationList) . "\n\n";

        $hostList = $oHost->getList($hostRelationList);
        $sql .= $oHost->generateSql($hostList) . "\n\n";

        $hostInformationList = $oHostInformation->getList($hostRelationList);
        $sql .= $oHostInformation->generateSql($hostInformationList) . "\n\n";

        $hostgroupRelationList = $oHostgroupRelation->getList($hostRelationList);
        $sql .= $oHostgroupRelation->generateSql($hostgroupRelationList) . "\n\n";

        $hostgroupList = $oHostgroup->getList($hostgroupRelationList);
        $sql .= $oHostgroup->generateSql($hostgroupList) . "\n\n";

        $hostCategoriesRelationList = $oHostCategoriesRelation->getList($hostRelationList);
        $sql .= $oHostCategoriesRelation->generateSql($hostCategoriesRelationList) . "\n\n";

        $hostCategoriesList = $oHostCategories->getList($hostCategoriesRelationList);
        $sql .= $oHostCategories->generateSql($hostCategoriesList) . "\n\n";

        $serviceRelationList = $oHostServiceRelation->getList($hostRelationList, $hostgroupRelationList);
        $sql .= $oHostServiceRelation->generateSql($serviceRelationList) . "\n\n";

        $serviceList = $oService->getList($serviceRelationList);
        $sql .= $oService->generateSql($serviceList) . "\n\n";

        $serviceInformationList = $oServiceInformation->getList($serviceRelationList);
        $sql .= $oServiceInformation->generateSql($serviceInformationList) . "\n\n";

        $servicegroupRelationList = $oServicegroupRelation->getList($serviceRelationList);
        $sql .= $oServicegroupRelation->generateSql($servicegroupRelationList) . "\n\n";

        $servicegroupList = $oServicegroup->getList($servicegroupRelationList);
        $sql .= $oServicegroup->generateSql($servicegroupList) . "\n\n";

        $serviceCategoriesRelationList = $oServiceCategoriesRelation->getList($serviceRelationList);
        $sql .= $oServiceCategoriesRelation->generateSql($serviceCategoriesRelationList) . "\n\n";

        $serviceCategoriesList = $oServiceCategories->getList($serviceCategoriesRelationList);
        $sql .= $oServiceCategories->generateSql($serviceCategoriesList) . "\n\n";

        $metaServiceRelationList = $oMetaServiceRelation->getList($hostRelationList);
        $sql .= $oMetaServiceRelation->generateSql($metaServiceRelationList) . "\n\n";

        $metaServiceList = $oMetaService->getList($metaServiceRelationList);
        $sql .= $oMetaService->generateSql($metaServiceList) . "\n\n";

        $metaContactList = $oMetaContact->getList($metaServiceRelationList);
        $sql .= $oMetaContact->generateSql($metaContactList) . "\n\n";

        $metaContactgroupList = $oMetaContactgroup->getList($metaServiceRelationList);
        $sql .= $oMetaContactgroup->generateSql($metaContactgroupList) . "\n\n";

        $contactHostRelationList = $oContactHostRelation->getList($hostRelationList);
        $sql .= $oContactHostRelation->generateSql($contactHostRelationList) . "\n\n";

        $contactServiceRelationList = $oContactServiceRelation->getList($serviceRelationList);
        $sql .= $oContactServiceRelation->generateSql($contactServiceRelationList) . "\n\n";

        $contactList = $oContact->getList($contactHostRelationList, $contactServiceRelationList);
        $sql .= $oContact->generateSql($contactList) . "\n\n";

        $ContactgroupRelationList = $oContactgroupContactRelation->getList($contactList);
        $sql .= $oContactgroupContactRelation->generateSql($ContactgroupRelationList) . "\n\n";

        $ContactgroupList = $oContactgroup->getList($ContactgroupRelationList);
        $sql .= $oContactgroup->generateSql($ContactgroupList) . "\n\n";

        $contactgroupHostRelationList = $oContactgroupHostRelation->getList($hostRelationList);
        $sql .= $oContactgroupHostRelation->generateSql($contactgroupHostRelationList) . "\n\n";

        $contactgroupHostgroupRelationList = $oContactgroupHostgroupRelation->getList($hostgroupRelationList);
        $sql .= $oContactgroupHostgroupRelation->generateSql($contactgroupHostgroupRelationList) . "\n\n";

        $contactgroupServiceRelationList = $oContactgroupServiceRelation->getList($serviceRelationList);
        $sql .= $oContactgroupServiceRelation->generateSql($contactgroupServiceRelationList) . "\n\n";

        $contactgroupServicegroupRelationList = $oContactgroupServicegroupRelation->getList($servicegroupRelationList);
        $sql .= $oContactgroupServicegroupRelation->generateSql($contactgroupServicegroupRelationList) . "\n\n";

        $aclResourcesHostList = $oAclResourcesHost->getList($hostRelationList);
        $sql .= $oAclResourcesHost->generateSql($aclResourcesHostList) . "\n\n";

        $aclResourcesHostCategorieList = $oAclResourcesHostCategorie->getList($hostCategoriesList);
        $sql .= $oAclResourcesHostCategorie->generateSql($aclResourcesHostCategorieList) . "\n\n";

        $aclResourcesHostexList = $oAclResourcesHostex->getList($hostRelationList);
        $sql .= $oAclResourcesHostex->generateSql($aclResourcesHostexList) . "\n\n";

        $aclResourcesHostgroupList = $oAclResourcesHostgroup->getList($hostgroupList);
        $sql .= $oAclResourcesHostgroup->generateSql($aclResourcesHostgroupList) . "\n\n";

        $aclResourcesServiceList = $oAclResourcesService->getList($serviceRelationList);
        $sql .= $oAclResourcesService->generateSql($aclResourcesServiceList) . "\n\n";

        $aclResourcesServiceCategorieList = $oAclResourcesServiceCategorie->getList($serviceCategoriesList);
        $sql .= $oAclResourcesServiceCategorie->generateSql($aclResourcesServiceCategorieList) . "\n\n";

        $aclResourcesServicegroupList = $oAclResourcesServicegroup->getList($servicegroupList);
        $sql .= $oAclResourcesServicegroup->generateSql($aclResourcesServicegroupList) . "\n\n";

        $aclResourcesMetaList = $oAclResourcesMeta->getList($metaServiceRelationList);
        $sql .= $oAclResourcesMeta->generateSql($aclResourcesMetaList) . "\n\n";

        $aclGroupContactsRelationList = $oAclGroupContactsRelation->getList($contactList);
        $sql .= $oAclGroupContactsRelation->generateSql($aclGroupContactsRelationList) . "\n\n";

        $aclGroupContactgroupsRelationList = $oAclGroupContactgroupsRelation->getList($ContactgroupList);
        $sql .= $oAclGroupContactgroupsRelation->generateSql($aclGroupContactgroupsRelationList) . "\n\n";

        $aclGroupsList = $oAclGroups->getList($aclGroupContactsRelationList, $aclGroupContactgroupsRelationList);
        $sql .= $oAclGroups->generateSql($aclGroupsList) . "\n\n";

        $aclGroupTopologyList = $oAclGroupTopology->getList($aclGroupsList);
        $sql .= $oAclGroupTopology->generateSql($aclGroupTopologyList) . "\n\n";

        $aclGroupActionsRelationList = $oAclGroupActionsRelation->getList($aclGroupsList);
        $sql .= $oAclGroupActionsRelation->generateSql($aclGroupActionsRelationList) . "\n\n";

        $aclActionsList = $oAclActions->getList($aclGroupActionsRelationList);
        $sql .= $oAclActions->generateSql($aclActionsList) . "\n\n";

        $aclActionsRulesList = $oAclActionsRules->getList($aclGroupActionsRelationList);
        $sql .= $oAclActionsRules->generateSql($aclActionsRulesList) . "\n\n";

        $aclResourcesGroupRelationList = $oAclResourcesGroupRelation->getList($aclGroupsList);
        $sql .= $oAclResourcesGroupRelation->generateSql($aclResourcesGroupRelationList) . "\n\n";

        $aclResourcesList = $oAclResources->getList($aclResourcesGroupRelationList);
        $sql .= $oAclResources->generateSql($aclResourcesList) . "\n\n";

        $sql .= $this->setForeignKey(1);

        $this->createFile($this->backend_instance->getPath());
        fwrite($this->fp, $sql);
        $this->close_file();
    }
    
    /**
     * 
     * @param int $status
     * @return string
     */
    protected function setForeignKey($status)
    {
        return 'SET FOREIGN_KEY_CHECKS = ' . $status . ';';
    }
}
