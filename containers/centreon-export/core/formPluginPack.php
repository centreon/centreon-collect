<?php
/*
 * Copyright 2005-2015 MERETHIS
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
 * As a special exception, the copyright holders of this program give MERETHIS
 * permission to link this program with independent modules to produce an executable,
 * regardless of the license terms of these independent modules, and to copy and
 * distribute the resulting executable under terms of MERETHIS choice, provided that
 * MERETHIS also meet, for each linked independent module, the terms  and conditions
 * of the license of that module. An independent module is a module which is not
 * derived from this program. If you modify this program, you may extend this
 * exception to your version of the program, but you are not obliged to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 *
 * For more information : contact@centreon.com
 *
 */

require_once realpath(dirname(__FILE__) . '/../centreon-export.conf.php');
require_once _CENTREON_PATH_ . '/www/class/centreonLocale.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonData.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonDB.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonHosttemplates.class.php';
require_once _CENTREON_PATH_ . '/www/class/centreonServicetemplates.class.php';

use \CentreonExport\Factory;
use \CentreonExport\Services\ListPluginPack;
use \CentreonExport\Services\ListTypeDiscovery;
use \CentreonExport\PluginPack;
use \CentreonExport\Command;
use \CentreonExport\Icon;
use \CentreonExport\HostTemplate;
use \CentreonExport\ServiceTemplate;
use \CentreonExport\Description;
use \CentreonExport\Dependency;
use \CentreonExport\Requirement;
use \CentreonExport\Mapper\MapperPluginPack;

if (!isset($oreon)) {
    exit();
}

$export = realpath(dirname(__FILE__));

$pearDB = new \CentreonDB();
$hostTemplateObject = new CentreonHosttemplates($pearDB);
$serviceTemplateObject = new CentreonServicetemplates($pearDB);

$aHostTemplate = $hostTemplateObject->getList(true, true);
$aServiceTemplate = $serviceTemplateObject->getList(true);

$exportFactory = new Factory();
$oPlugin = $exportFactory->newPluginPack();
$tagObject = $exportFactory->newTag();
$oPluginMapper = $exportFactory->newMapperPluginPack();

$id = filter_input(INPUT_GET, 'id', FILTER_VALIDATE_INT);
if (!empty($id)) {
    $oPlugin->getDetail($id, $aHostTemplate, $aServiceTemplate);
}
$id = trim($oPlugin->getPlugin_id());
$tags = $tagObject->getTagsByPlugin($id);
$defaultTags = array();
foreach ($tags as $tag) {
    $defaultTags[$tag['text']] = $tag['id'];
}

# Set default values for description
$defaultDescriptions = $oPlugin->getPlugin_description();

# Set form default values
$default = array(
    "plugin_id" => $oPlugin->getPlugin_id(),
    "name" => $oPlugin->getPlugin_name(),
    "display_name" => $oPlugin->getPlugin_Display_Name(),
    "status" => $oPlugin->getPlugin_status(),
    "author" => $oPlugin->getPlugin_author(),
    "email" => $oPlugin->getPlugin_email(),
    "website" => $oPlugin->getPlugin_website(),
    "compatibility" => $oPlugin->getPlugin_compatibility(),
    "description" => $defaultDescriptions,
    "monitoring_procedure" => $oPlugin->getPlugin_monitoring_procedure(),
    "type_discovery" => $oPlugin->getPlugin_discovery_category(),
    "version" => $oPlugin->getPlugin_version(),
    "id_icon"   => $oPlugin->getPlugin_icon()
);

$cdata = \CentreonData::getInstance();

//Preset values of Dependance
$oDependency = new Dependency();
$aDependency = $oDependency->getListByPlugin($oPlugin->getPlugin_id());
$cdata->addJsData('clone-values-dependency', htmlspecialchars(
                json_encode($aDependency), ENT_QUOTES
        )
);
$cdata->addJsData('clone-count-dependency', count($aDependency));

//Preset values of requirement
$oRequirement = new Requirement();
$aRequirement = $oRequirement->getListByPlugin($oPlugin->getPlugin_id());
$cdata->addJsData('clone-values-requirement', htmlspecialchars(
                json_encode($aRequirement), ENT_QUOTES
        )
);
$cdata->addJsData('clone-count-requirement', count($aRequirement));

/*
 * Smarty template Init
 */
$tpl = new \Smarty();
$tpl = initSmartyTpl($path, $tpl);

/*
 * Template / Style for Quickform input
 */
$attrsText    = array("size"=>"30");
$attrsTextSmall    = array("size"=>"10");
$attrsText2    = array("size"=>"60");
$attrsAdvSelect = array("style" => "width: 300px; height: 250px;");
$attrsTextarea    = array("rows"=>"5", "cols"=>"40");
$template    = "<table><tr><td>{unselected}</td><td align='center'>{add}<br /><br /><br />{remove}</td><td>{selected}</td></tr></table>";

/*
 * pool basic information
 */
$form = new \HTML_QuickForm('Form', 'post', "?p=" . $p."&o=".$o);
$form->setRequiredNote("<i style='color: red;'>*</i>&nbsp;" . _("Required fields"));

if ($o == "a") {
    $form->addElement('header', 'title', _("Add a plugin"));
} elseif ($o == "e") {
    $form->addElement('hidden', 'plugin_id');
    $form->addElement('header', 'title', _("Modify a plugin pack"));
}

$form->addElement('text', 'name', _("Name"), $attrsText2);
$form->registerRule('existPlugin', 'callback', 'testPluginExistence');
$form->addRule('name', _("Compulsory Name"), 'required');
$form->addRule('name', _("Plugin pack name is already in use"), 'existPlugin');

$form->addElement('text', 'display_name', _("Display Name"), $attrsText);
$form->addRule('display_name', _("Compulsory Display Name"), 'required');

$status_type = array('stable' => _('Stable'),
                    'testing' => _('Testing'),
                    'dev' => _('Development'),
                    'experimental' => _('Experimental'),
                    'deprecated' => _('Deprecated'));

$form->addElement('select', 'status', _('Status'), $status_type);

$oListTypeDiscovery = new ListTypeDiscovery();
$aTypDiscovery = $oListTypeDiscovery->getList(array());

$form->addElement('select', 'type_discovery', _('Discovery type'), $aTypDiscovery);

$form->addElement('text', 'author', _("Author"), $attrsText2);
$form->addElement('text', 'email', _("Email"), $attrsText2);
$form->addElement('text', 'website', _("Web site"), $attrsText2);

$attrTags =  array(
    'datasourceOrigin' => 'ajax',
    'multiple' => true,
    'defaultDataset' => $defaultTags,
    'availableDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_export_tags&action=all'
);

$form->addElement('tags', 'tag', _("Tags"), array(), $attrTags);

$form->addElement('textarea', 'compatibility', _("Compatibility"), $attrsTextarea);
$form->addElement('textarea', "monitoring_procedure", _("Monitoring procedure"), array_merge($attrsTextarea, array('class'=>'docExport_Textarea')));

$cloneRequirement = array();
$cloneRequirement[] = $form->addElement(
    'text', 'nameRequirement[#index#]', _('Name'), array(
    'id' => 'nameRequirement_#index#',
    'size' => 25
    )
);
$cloneRequirement[] = $form->addElement(
    'text', 'versionRequirement[#index#]', _('Version'), array(
    'id' => 'versionRequirement_#index#',
    'size' => 25
    )
);

$oPluginList = new ListPluginPack();
$cloneDependency = array();
$aPluginList = $oPluginList->getAll($oPlugin->getPlugin_slug());
array_unshift($aPluginList, "");
$cloneDependency[] = $form->addElement(
    'select', 'nameDependency[#index#]', _("Plugin"), $aPluginList, array(
        "id" => "nameDependency_#index#",
        "type" => "select-one"
    )
);

$cloneDependency[] = $form->addElement(
    'select', 'versionDependency[#index#]', _('Version'), array(), array(
        'id' => 'versionDependency_#index#'
    )
);

$localeObject = new \CentreonLocale(new \CentreonDB());
$localeList = $localeObject->getLocaleList();
$tpl->assign('descriptionLabel', _("Description"));
$tpl->assign('localeList', $localeList);
$tpl->assign('previewLabel', _("Preview"));
foreach ($localeList as $locale) {
    $form->addElement('textarea', "description[" . $locale["locale_short_name"] . "]", _("Description"), $attrsTextarea);
}

$tpl->assign('dir_tpl', realpath(dirname(__FILE__)) . '/templates/');
$tpl->assign('requirement_label', _('Software Requirement'));
$tpl->assign('cloneRequirement', $cloneRequirement);
$tpl->assign('dependency_label', _('Dependencies'));
$tpl->assign('cloneDependency', $cloneDependency);

$tpl->assign("tab1", _("Plugin Pack Configuration"));
$tpl->assign("tab2", _("Inclusions / Exclusions"));
$tpl->assign("tab3", _("Documentation"));
$tpl->assign("basic_info_label", _("Basic informations"));
$tpl->assign("basic_credentials_label", _("Credentials"));
$tpl->assign("basic_discovery_label", _("Discovery"));
$tpl->assign("basic_requirement_label", _("Requirement"));
$tpl->assign("search", _("search").'...');

$tpl->assign("discovery_command_message", _("Wrong value"));

$attrCommand =  array(
    'datasourceOrigin' => 'ajax',
    'multiple' => true,
    'linkedObject' => 'centreonCommand',
    'defaultDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_export_command&action=list&target=command&field=command&id=' . $oPlugin->getPlugin_id(),
    'availableDatasetRoute' => './include/common/webServices/rest/internal.php?object=centreon_configuration_command&action=list&t=2'
);

$form->addElement('select2', 'command', _("Command"), array(), $attrCommand);
$form->addElement('header', 'inclusion', _("Inclusions"));


// Setting Host Template (included and excluded
$availableHostTemplate = $aHostTemplate;
$excludedHostTplCheckbox = array();
$includedHostTplCheckbox = array();
$hostTemplateIncludedParameters = $oPlugin->getHostIncludedParameters();

$checkBoxTemplate = '<div class="checkbox-tpl"><span class="editBt disabled" id="span_{id}"></span>{element}</div>';

// Excluded HostTpl
$selectedHostTemplateExcluded = $oPlugin->getHostExcluded();
$hostTemplateExcludedId = array();
$indexExcludedHost = 0;
foreach ($selectedHostTemplateExcluded as $id => $name) {
    $excludedHostTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"excluded_host_".$id, "class"=> "excluded host ".strtolower($name), "data-place" => $indexIncludedHost));
    $excludedHostTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplate);    
    $excludedHostTplCheckbox[$id]->setChecked(true);
    $indexExcludedHost++;
}

// Included HostTpl
$selectedHostTemplateIncluded = $oPlugin->getHostIncluded();
$hostTemplateIncludedId = array();
$indexIncludedHost = 0;
foreach ($selectedHostTemplateIncluded as $id => $name) {
    $includedHostTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"included_host_".$id, "class"=> "included host ".strtolower($name), "data-place" => $indexIncludedHost));
    $includedHostTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplate);
    
    $discovery_protocol = '';
    $discovery_category = '';
    $discovery_command = '';
    $discovery_documentation = '';
    if (isset($hostTemplateIncludedParameters[$id])) {
        $discovery_protocol .= $hostTemplateIncludedParameters[$id]['discovery_protocol'];
        $discovery_category .= $hostTemplateIncludedParameters[$id]['discovery_validator'];
        $discovery_command .= $hostTemplateIncludedParameters[$id]['discovery_command'];
        $discovery_documentation .= $hostTemplateIncludedParameters[$id]['discovery_documentation'];
    }
    $form->addElement('hidden', "protocol_included_host[$id]", $discovery_protocol, array("id"=>"protocol_included_host_".$id));
    $form->addElement('hidden', "discovery_category_included_host[$id]", $discovery_category, array("id"=>"discovery_category_included_host_".$id));
    $form->addElement('hidden', "discovery_command_included_host[$id]", $discovery_command, array("id"=>"discovery_command_included_host_".$id));
    $form->addElement('hidden', "documentation_included_host[$id]", $discovery_documentation, array("id"=>"documentation_included_host_".$id));
    $includedHostTplCheckbox[$id]->setChecked(true);
    $indexIncludedHost++;
}

foreach ($availableHostTemplate as $id => $name) {
    if (!in_array($name, $selectedHostTemplateExcluded) && !in_array($name, $selectedHostTemplateIncluded)) {
        $excludedHostTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"excluded_host_".$id, "class"=> "excluded host ".strtolower($name), "data-place" => $indexExcludedHost));
        $excludedHostTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplate);
        $includedHostTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"included_host_".$id, "class"=> "included host ".strtolower($name), "data-place" => $indexIncludedHost));
        $includedHostTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplate);

        $discovery_protocol = '';
        $discovery_category = '';
        $discovery_command = '';
        $discovery_documentation = '';
        if (isset($hostTemplateIncludedParameters[$id])) {
            $discovery_protocol .= $hostTemplateIncludedParameters[$id]['discovery_protocol'];
            $discovery_category .= $hostTemplateIncludedParameters[$id]['discovery_validator'];
            $discovery_command .= $hostTemplateIncludedParameters[$id]['discovery_command'];
            $discovery_documentation .= $hostTemplateIncludedParameters[$id]['discovery_documentation'];
        }
        $form->addElement('hidden', "protocol_included_host[$id]", $discovery_protocol, array("id"=>"protocol_included_host_".$id));
        $form->addElement('hidden', "discovery_category_included_host[$id]", $discovery_category, array("id"=>"discovery_category_included_host_".$id));
        $form->addElement('hidden', "discovery_command_included_host[$id]", $discovery_command, array("id"=>"discovery_command_included_host_".$id));
        $form->addElement('hidden', "documentation_included_host[$id]", $discovery_documentation, array("id"=>"documentation_included_host_".$id));
        $indexExcludedHost++;
        $indexIncludedHost++;
    }
}

$form->addGroup($excludedHostTplCheckbox, 'hostExcluded', _("Excluded Host Templates"), '');
$form->addGroup($includedHostTplCheckbox, 'hostIncluded', _("Included Host Templates"), '');

// HostParameters Form
$form->addElement('text', 'htpl_discovery_protocol',  _('Protocol'));
$form->addElement('select', 'htpl_discovery_category', _('Categories'), $aTypDiscovery);
$form->addElement('text', 'htpl_discovery_command', _('Discovery Command'));
$form->addElement('text', 'htpl_documentation', _('Documentation'));
$form->addElement('hidden', 'elem_id', '', array("id"=> 'elem_id'));

$checkBoxTemplateService = '<div class="checkbox-servicetpl"><span class="editBt disabled" id="spanService_{id}"></span>{element}</div>';

// Setting Service Template (included and excluded)
$availableServiceTemplate = $aServiceTemplate;
$excludedServiceTplCheckbox = array();
$includedServiceTplCheckbox = array();
$indexExcludedService = 0;
$indexIncludedService = 0;
$selectedServiceTemplateIncluded = $oPlugin->getServiceIncluded();

// Excluded ServiceTpl
$selectedServiceTemplateExcluded = $oPlugin->getServiceExcluded();
$serviceTemplateExcludedId = array();
foreach ($selectedServiceTemplateExcluded as $id => $name) {
    $excludedServiceTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"excluded_service_".$id, "class"=>"excluded service ".strtolower($name), "data-place" => $indexExcludedService));
    $excludedServiceTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplateService);    
    $excludedServiceTplCheckbox[$id]->setChecked(true);
    $indexExcludedService++;
}

// Included ServiceTpl
$selectedServiceTemplateIncluded = $oPlugin->getServiceIncluded();
foreach ($selectedServiceTemplateIncluded as $id => $svc) {
    $includedServiceTplCheckbox[$svc['id']] = \HTML_QuickForm::createElement('customcheckbox', $svc['id'], '&nbsp;', $svc['name'], array("id"=>"included_service_".$svc['id'], "class"=>"included service ".strtolower($svc['name']), "data-place" => $indexIncludedService));
    $includedServiceTplCheckbox[$svc['id']]->setCheckboxTemplate($checkBoxTemplateService);
    $includedServiceTplCheckbox[$svc['id']]->setChecked(true);
    $indexIncludedService++;
    $discovery_command = '';
    if (isset($selectedServiceTemplateIncluded[$svc['id']])) {
        $discovery_command .= $selectedServiceTemplateIncluded[$svc['id']]['discovery_command'];
    }
    $i = $svc['id'];
    $form->addElement('hidden', "discovery_command_included_service[$i]", $discovery_command, array("id"=>"discovery_command_included_service_".$svc['id']));
}

foreach ($availableServiceTemplate as $id => $name) {
    if (!in_array($name, $selectedServiceTemplateExcluded) && !in_array_r($name, $selectedServiceTemplateIncluded)) {
        $excludedServiceTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"excluded_service_".$id, "class"=>"excluded service ".strtolower($name), "data-place" => $indexExcludedService));
        $excludedServiceTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplateService);
        $includedServiceTplCheckbox[$id] = \HTML_QuickForm::createElement('customcheckbox', $id, '&nbsp;', $name, array("id"=>"included_service_".$id, "class"=>"included service ".strtolower($name), "data-place" => $indexIncludedService));
        $includedServiceTplCheckbox[$id]->setCheckboxTemplate($checkBoxTemplateService);

        $discovery_command = '';
        if (isset($selectedServiceTemplateIncluded[$id])) {
            $discovery_command .= $selectedServiceTemplateIncluded[$id]['discovery_command'];
        }
        $form->addElement('hidden', "discovery_command_included_service[$id]", $discovery_command, array("id"=>"discovery_command_included_service_".$id));
        $indexExcludedService++;
        $indexIncludedService++;
    }
}


$form->addGroup($excludedServiceTplCheckbox, 'serviceExcluded', _("Excluded Service Templates"), '');
$form->addGroup($includedServiceTplCheckbox, 'serviceIncluded', _("Included Service Templates"), '');

// ServiceTplParameters Form
$form->addElement('text', 'stpl_discovery_command', _('Discovery Command'));
$form->addElement('hidden', 'svc_elem_id', '', array("id"=> 'svc_elem_id'));

$form->addElement('header', 'exclusion', _("Exclusions"));

$form->addElement('text', 'name', _("Name"), $attrsText2);
$file = $form->addElement('file', 'icon', _("Icon"));
$form->addElement('hidden', 'id_icon');
$form->addElement('button', 'clear', _("Clear Icon"), array('id' => 'clear', "class" => "btc bt_danger"));

$form->addElement('submit', 'submitA', _("Save"), array("class" => "btc bt_success"));

$form->setDefaults($default);
$valid = false;



if ($form->validate()) {
    $valid = true;
    $rempli = false;
    foreach ($form->_elements as $ele) {
        if (preg_match("#^description#", $ele->getName())) {
            $val = trim($form->getSubmitValue($ele->getName()));
            if (!empty($val)) {
                $rempli = true;
            }
        }
    }

    if (!$rempli) {
        print("<div class='msg' align='center'>"._("Impossible to validate, you must enter at least one description")."</div>");
        $valid = false;
    }
    if ($valid) {
    $o = null;
    $oPlugin = $oPluginMapper->saveInObject($form);
    
    if (strlen($oPlugin->getPlugin_icon()) == 0) {
        $oIcon = new Icon();
        $oIcon->deleteIconByPlugin($oPlugin->getPlugin_id());
        $iIdIcon = $oIcon->uploadIcon($file);
        if ($iIdIcon > 0) {
            $oPlugin->setPlugin_icon($iIdIcon);
        }
    }
           
    if (is_null($oPlugin->getPlugin_id())) {
        $Id = $oPlugin->insert();
        $oPlugin->setPlugin_id($Id);
    } else {
        $oPlugin->update();
    }
    
    $oCommand = new Command();
    $oCommand->addCommandInPlugin($oPlugin->getPlugin_id(), $oPlugin->getCommand());

    $tagObject->addTagsInPlugin($oPlugin->getPlugin_id(), $oPlugin->getPlugin_tags());
    
    $oDependency = new Dependency();
    $oDependency->addDependenciesInPlugin($oPlugin->getPlugin_id(), $oPlugin->getPlugin_dependencies());
    
    $oRequirement = new Requirement();
    $oRequirement->addRequirementInPlugin($oPlugin->getPlugin_id(), $oPlugin->getPlugin_requirement());
    
    $aHostLinked= array();
    foreach ($oPlugin->getHostExcluded() as $value => $label) {
        $aHostLinked[] = array(
            'host_id' => $value,
            'status' => 0
        );
    }
    foreach ($oPlugin->getHostIncluded() as $value => $label) {
        $aHostLinked[] = array(
            'host_id' => $value,
            'status' => 1
        );
    }
    $oHostTemplate = new HostTemplate();
    $oHostTemplate->addHostTemplateInPlugin($oPlugin->getPlugin_id(), $aHostLinked, $oPlugin->getHostIncludedParameters());
    
    $aServiceLinked= array();
    foreach ($oPlugin->getServiceExcluded() as $value => $label) {
        $aServiceLinked[] = array(
            'service_id' => $value,
            'status' => 0
        );
    }
    $aServiceIncludedParameters = $oPlugin->getServiceIncludedParameters();

    foreach ($oPlugin->getServiceIncluded() as $value => $label) {
        $aServiceLinked[] = array(
            'service_id' => $value,
            'status' => 1,
            'discovery_command' => isset($aServiceIncludedParameters[$value]) ? $aServiceIncludedParameters[$value] : ''
        );
    }
    
    $oServiceTemplate = new ServiceTemplate();
    $oServiceTemplate->addServiceTemplateInPlugin($oPlugin->getPlugin_id(), $aServiceLinked);
}
}


if ($valid) {
    require_once($export . "/listPluginPack.php");
} else {
    $iIdIcon = $oPlugin->getPlugin_icon();
    if (!empty($iIdIcon)) {
        $oIcon = new Icon();
        $sFile = $oIcon->getIcon($iIdIcon);
        $tpl->assign('image', $sFile);
    }
    
    $renderer = new \HTML_QuickForm_Renderer_ArraySmarty($tpl);
    $renderer->setRequiredTemplate('{$label}&nbsp;<font color="red" size="1">*</font>');
    $renderer->setErrorTemplate('<font color="red">{$error}</font><br />{$html}');
    $form->accept($renderer);
    $tpl->assign('form', $renderer->toArray());
    $tpl->assign('max_uploader_file', ini_get("upload_max_filesize"));
    $tpl->display($export . "/templates/formPluginPack.tpl");
}
