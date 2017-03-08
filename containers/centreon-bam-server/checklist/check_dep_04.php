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

$licenseFile = '<input id="fileuploadBAM" type="file" name="licensefile" />
			<script type="text/javascript">
				jQuery(\'#fileuploadBAM\').fileupload({
						url: \'include/options/oreon/modules/licenseUpload.php?module=centreon-bam-server\',
						maxFileSize: 5000000,
						type: \'POST\',
						dataType: \'text\',
						autoUpload: true,
						done: function (e, data) {
							myResponse = data.jqXHR;
							
							if (myResponse.responseText)
								alert(myResponse.responseText);
							else
								alert(data.result);
								
							jQuery(\'#\' + modalBoxId).dialog(\'close\');
							CheckModule();
						}
					});
			</script>
	';
$myZendIds = zend_get_id();
$licenseFile .= '<br /><br />Here the serial(s) number(s) for your plateform : <br />&nbsp;&nbsp;&nbsp;&nbsp;'.implode("<br />&nbsp;nbsp;&nbsp;&nbsp;", $myZendIds);
$customAction = array('action' => $licenseFile, 'name' => 'Centreon Bam License Upload');

$delaiAlerte = 15 * 24 * 60 * 60;
if (function_exists('zend_loader_enabled')) {
	if(file_exists($centreon_path . "www/modules/" . $filename . "/license/merethis_lic.zl")) {
		if (zend_loader_file_encoded($centreon_path . "www/modules/" . $filename . "/license/merethis_lic.zl")) {
			$zend_info = zend_loader_file_licensed($centreon_path . "www/modules/" . $filename . "/license/merethis_lic.zl");
		} else {
			$zend_info = parse_zend_license_file($centreon_path . "www/modules/" . $filename . "/license/merethis_lic.zl");
		}
		
		$license_expires = strtotime($zend_info['Expires']);
		if ($license_expires < time()) {
			$critical = true;
			$message[] = array('ErrorMessage' => 'The license has expired',
							   'Solution' => 'Please upload your new License here'.$licenseFile);
		} else if($license_expires < ($license_expires - $delaiAlerte)) {
			$warning = true;
			$message[] = array('ErrorMessage' => 'The license will expire in less than 15 days',
							   'Solution' => 'Please issue and upload your new License here'.$licenseFile);
		}
	} else {
		$critical = true;
		$message[] = array('ErrorMessage' => 'No license found',
						   'Solution' => 'Please upload your new License here'.$licenseFile);
	}
}
