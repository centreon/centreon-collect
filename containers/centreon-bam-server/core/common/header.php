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

if (!file_exists($centreon_path.'www/modules/centreon-bam-server/core/class/CentreonBam/License.php')) {
    throw new Exception("Enable to load Class License.php");
}
require_once $centreon_path.'www/modules/centreon-bam-server/core/class/CentreonBam/License.php';
$sFileLicense = $centreon_path . "www/modules/centreon-bam-server/license/merethis_lic.zl";

if (CentreonBam_License::checkValidity($sFileLicense) == false) {
    echo "<div class='msg' align='center'>";
    echo _("Centreon BAM license is not valid. Please contact your administrator for more information.");
    echo "</div>";
    print "\t\t\t</td>\t\t</tr>\t</table>\n</div><!-- end contener -->";
    include("./footer.php");
    exit;
}

if (!isset($oreon)) {
	if (is_file($centreon_path . 'www/class/centreon.class.php')) {
        require_once $centreon_path . 'www/class/centreon.class.php';
    }
    if (is_file($centreon_path . 'www/class/centreonSession.class.php')) {
        require_once $centreon_path . 'www/class/centreonSession.class.php';
    }
    if (is_file($centreon_path . 'www/class/centreonUser.class.php')) {
        require_once $centreon_path . 'www/class/centreonUser.class.php';
    }
    session_start();
	if (isset($_SESSION['oreon'])) {
	    $oreon = $_SESSION['oreon'];
	} elseif (isset($_SESSION['centreon'])) {
	    $oreon = $_SESSION['centreon'];
	}
}
if (isset($oreon)) {
	try {
	    require_once($centreon_path . "www/modules/centreon-bam-server/core/class/CentreonBam/Loader.php");
		$cLoad = new CentreonBam_Loader($centreon_path.'www/modules/centreon-bam-server/core/class/Centreon/');
		$cLoadBam = new CentreonBam_Loader($centreon_path.'www/modules/centreon-bam-server/core/class/CentreonBam/');
	}
	catch (Exception $e) {
	    echo $e->getMessage();
	}
	$cLang = new CentreonBam_Lang($centreon_path, $oreon);
	$cLang->bindLang();
}
if (isset($oreon)) {
	$pearDB = new CentreonBam_Db();
	$query = "SELECT * FROM session WHERE session_id = '".session_id()."' AND user_id = '".$oreon->user->user_id."'";
	$DBRES = $pearDB->query($query);
	if (!$DBRES->numRows()) {
		exit();
	}
} else {
	exit();
}
