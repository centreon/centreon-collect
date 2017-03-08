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

use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\Ba;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BamAcl;
use CentreonPollerDisplayCentral\ConfigGenerate\Bam\BamUserPreferences;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaBvRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaCgRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaChildrenRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaEscalationRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaParentsRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaPollerRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaTimeperiodRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\BaUserOverViewRelations;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\Bv;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\Boolean;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\Impacts;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\Kpi;
use \CentreonPollerDisplayCentral\ConfigGenerate\Bam\KpiView;


/**
 * User: kduret
 * Date: 23/02/2017
 * Time: 09:19
 */
class Bam extends \AbstractObject
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
     * @return boolean
     */
    public function isBamModuleAvailable()
    {
        $bamAvailable = false;
        
        $querySelectBamModule = "SELECT id FROM modules_informations WHERE name = 'centreon-bam-server' LIMIT 1";
        $resSelectBamModule = $this->backend_instance->db->query($querySelectBamModule);
        
        $rowSelectModule = $resSelectBamModule->fetch();
        
        if (count($rowSelectModule) > 0) {
            $bamAvailable = true;
        }
        
        return $bamAvailable;
    }
    
    /**
     * 
     * @param type $poller_id
     */
    public function generateObjects($poller_id)
    {
        $sql = '';
        $filteredObjects = array();
        
        // Disable MySQL foreign key check
        $sql .= $this->setForeignKey(0). "\n\n";
        
        $impactsObj = new Impacts($this->backend_instance->db, $pollerId);
        $impactsList = $impactsObj->getList();
        $sql .=  $impactsObj->generateSql($impactsList) . "\n\n";
        
        // Generate BAM Poller relation
        $baPollerObj = new BaPollerRelations($this->backend_instance->db, $poller_id);
        $baPollerList = $baPollerObj->getList();
        $sql .=  $baPollerObj->generateSql($baPollerList) . "\n\n";
        $baSimpleList = $this->getSimpleObjectList($baPollerList, 'ba_id');
        
        // Generate BA
        $baObj = new Ba($this->backend_instance->db, $poller_id);
        $baList = $baObj->getList(array('ba_id' => $baSimpleList));
        $sql .=  $baObj->generateSql($baList) . "\n\n";
        
        // Generate Ba-BV relation
        $baBvObj = new BaBvRelations($this->backend_instance->db, $poller_id);
        $baBvList = $baBvObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $baBvObj->generateSql($baBvList) . "\n\n";
        $bvSimpleList = $this->getSimpleObjectList($baBvList, 'id_ba_group');
        
        // Generate Ba-Cg relation
        $baCgObj = new BaCgRelations($this->backend_instance->db, $poller_id);
        $baCgList = $baCgObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $baCgObj->generateSql($baCgList) . "\n\n";
        
        // Generate Ba Children relation
        $baChildrenObj = new BaChildrenRelations($this->backend_instance->db, $poller_id);
        $baChildrenList = $baChildrenObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $baChildrenObj->generateSql($baChildrenList) . "\n\n";
        
        // Generate Ba Parent relation
        $baParentsObj = new BaParentsRelations($this->backend_instance->db, $poller_id);
        $baParentsList = $baParentsObj->getList(array('id_dep' => $baSimpleList));
        $sql .=  $baParentsObj->generateSql($baParentsList) . "\n\n";
        
        // Generate Ba Escalation relation
        $baEscalationsObj = new BaEscalationRelations($this->backend_instance->db, $poller_id);
        $baEscalationsList = $baEscalationsObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $baEscalationsObj->generateSql($baEscalationsList) . "\n\n";
        
        // Generate Ba Timeperiod relation
        $baTimeperiodObj = new BaTimeperiodRelations($this->backend_instance->db, $poller_id);
        $baTimeperiodList = $baTimeperiodObj->getList(array('ba_id' => $baSimpleList));
        $sql .=  $baTimeperiodObj->generateSql($baTimeperiodList) . "\n\n";
        
        // Generate Ba User Overview relation
        $baUserOverviewObj = new BaUserOverViewRelations($this->backend_instance->db, $poller_id);
        $baUserOverviewList = $baUserOverviewObj->getList(array('ba_id' => $baSimpleList));
        $sql .=  $baUserOverviewObj->generateSql($baUserOverviewList) . "\n\n";
        
        // Generate BV
        $bvObj = new Bv($this->backend_instance->db, $poller_id);
        $bvList = $bvObj->getList(array('id_ba_group' => $bvSimpleList));
        $sql .=  $bvObj->generateSql($bvList) . "\n\n";
        
        // Generate usual Kpi
        $kpiObj = new Kpi($this->backend_instance->db, $poller_id);
        $kpiList = $kpiObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $kpiObj->generateSql($kpiList) . "\n\n";
        $booleanKpiSimpleList = $this->getSimpleObjectList($kpiList, 'boolean_id');
        
        //
        $kpiViewObj = new KpiView($this->backend_instance->db, $poller_id);
        $kpiViewList = $kpiViewObj->getList(array('id_ba' => $baSimpleList));
        $sql .=  $kpiViewObj->generateSql($kpiViewList) . "\n\n";
        
        // Generate boolean Kpi
        $booleanKpiObj = new Boolean($this->backend_instance->db, $poller_id);
        $booleanKpiList = $booleanKpiObj->getList($booleanKpiSimpleList);
        $sql .=  $booleanKpiObj->generateSql($booleanKpiList) . "\n\n";
        
        // Generate Bam ACL
        $bamAcl = new BamAcl($this->backend_instance->db, $poller_id);
        $bamAclList = $bamAcl->getList(array('ba_group_id' => $bvSimpleList));
        $sql .=  $bamAcl->generateSql($bamAclList) . "\n\n";
        
        // Generate Bam ACL
        $bamUserPreferences = new BamUserPreferences($this->backend_instance->db, $poller_id);
        $bamuserPreferencesList = $bamUserPreferences->getList();
        $sql .=  $bamUserPreferences->generateSql($bamuserPreferencesList) . "\n\n";
        
        // Enable MySQL foreign key check
        $sql .= $this->setForeignKey(1). "\n\n";
        
        $this->createFile($this->backend_instance->getPath());
        fwrite($this->fp, $sql);
        $this->close_file();
    }
    
    /**
     * 
     * @param array $objectList
     * @param string $key
     */
    protected function getSimpleObjectList($objectList, $key)
    {
        $simpleList = array();
        foreach ($objectList as $object) {
            if (isset($object[$key])) {
                $simpleList[] = $object[$key];
            }
        }
        return $simpleList;
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
