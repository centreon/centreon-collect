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
