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
require_once "HTML/QuickForm.php";
require_once 'HTML/QuickForm/advmultiselect.php';
require_once 'HTML/QuickForm/Renderer/ArraySmarty.php';

require_once realpath(dirname(__FILE__) . '/../../centreon-pluginpack-manager.conf.php');
$module_path = $centreon_path . "www/modules/centreon-pp-manager/";
$path = "./modules/centreon-pp-manager/core/templates/";
include_once $module_path.'/core/class/centreonDBManager.class.php';
include_once $module_path.'/core/class/License.class.php';
require_once $module_path . '/core/class/PluginPackManagerFactory.php';
include_once $module_path.'/core/class/ImpApi.class.php';
include_once $centreon_path . '/www/class/centreonRestHttp.class.php';
include_once $module_path.'/core/manager/functions.php';

$categories = getCategories($pearDB);

$status_type = array(
    'all' => _('All'),
    'stable' => _('Stable'),
    'testing' => _('Testing'),
    'dev' => _('Development'),
    'experimental' => _('Experimental'),
    'deprecated' => _('Deprecated')
);

// Sort by key (because need value 'all' in first by default in dropdown)
ksort($status_type);

$aOperators = array(
    ''   => '',
    'gt'  => '>',
    'lt'  => '<',
);

if (!isset($_SESSION['centreon'])) {
    CentreonSession::start();
}

if (isset($_SESSION['centreon'])) {
    $centreon = $_SESSION['centreon'];
} else {
    exit;
}

if (isset($_GET['num']) && filter_input(INPUT_GET, 'num', FILTER_SANITIZE_NUMBER_INT)) {
    $num = $_GET['num'];
} else {
    $num = 0;
}

$name = null;
$category = null;
$status = null;
$lastUpdate = null;
$operator = null;

if (isset($_POST)) {
    $name  = filter_input(INPUT_POST, "name", FILTER_SANITIZE_STRING, FILTER_FLAG_NO_ENCODE_QUOTES);
    $category  = filter_input(INPUT_POST, "category", FILTER_SANITIZE_STRING);
    $status  = filter_input(INPUT_POST, "status", FILTER_SANITIZE_STRING);

    if (!empty($_POST['lastUpdate']) && !empty($_POST['operator'])) {
        $lastUpdate = $_POST['lastUpdate'];
        $operator   = $_POST['operator'];
    }
}

$locale = $centreon->user->get_lang();
$parsedLocale = locale_parse($locale);
$localeShortName = isset($parsedLocale['language']) ? $parsedLocale['language'] : 'en';
putenv("LANG=$locale");
setlocale(LC_ALL, $locale);
bindtextdomain("messages", $module_path . "locale/");
bind_textdomain_codeset("messages", "UTF-8");
textdomain("messages");

?>
<script type="text/javascript">
    var locale = "<?php echo $localeShortName;?>";
    var name = "<?php echo addslashes($name);?>";
    var category = "<?php echo $category;?>";
    var status = "<?php echo $status;?>";
    var lastUpdate = "<?php echo $lastUpdate;?>";
    var operator = "<?php echo $operator;?>";

    // Locales for checkRemovePluginPack() :
    var usedMessageStart = "<?php echo _("Sorry, you can't delete this plugin pack because it is used on :");?>";
    var usedMessageHost = "<?php echo _("Host(s)");?>";
    var usedMessageService = "<?php echo _("Service(s)");?>";
    var usedMessageCommand = "<?php echo _("Command(s)");?>";
    var usedMessageHostTpl = "<?php echo _("Host template(s)");?>";
    var usedMessageServiceTpl = "<?php echo _("Service template(s)");?>";
</script>
<?php

$tpl = new Smarty();
$tpl = initSmartyTpl($path, $tpl);

$labels = array();
$labels['title'] = _("Title");
$labels['title_page'] = _("Plugins Packs Manager");
$labels['description'] = _("Description");
$labels['version'] = _("Version");
$labels['release'] = _("Release");
$labels['status'] = _("Status");
$labels['nb_htpl'] = _("Number of host templates");
$labels['nb_stpl'] = _("Number of service templates");
$labels['nb_cmd'] = _("Number of commands");
$labels['actions'] = _("Actions");
$labels['view_more'] = _("View more");
$labels['more'] = _("More");
$labels['confirm'] = _("You are about to uninstall the plugin pack, this will 
    also delete its associated templates. Do you still want to proceed?");
$labels['stillInUse'] = _("Some templates are still in use! Replace them then 
    try again.");

$tpl->assign('status_type', $status_type);
$tpl->assign('categories', $categories);
$tpl->assign('aOperators', $aOperators);
$tpl->assign('lastUpdate', _("Last update"));
$tpl->assign('labels', $labels);

/* @todo get the company token */

$license = false;
$subscription = false;

if (\CentreonPluginPackManager\License::isValid()) {
    $license = true;
} else {
    try {
        $pluginPackManagerFactory = new \PluginPackManagerFactory(new CentreonDB());
        $ImpApi = $pluginPackManagerFactory->newImpApi(CENTREON_IMP_API_URL);
        $subscription = $ImpApi->checkSubscription();
    } catch (\Exception $e) {

    }
}

$tpl->assign('isAuth', $subscription);
$tpl->assign('hasLicense', $license);

$tpl->display("list-2.7.tpl");
