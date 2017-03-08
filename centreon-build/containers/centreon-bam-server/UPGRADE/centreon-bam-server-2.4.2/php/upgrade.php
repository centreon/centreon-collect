<?php
$db = new CentreonDB();
$sql = "SELECT name FROM giv_components_template WHERE name = '".$db->escape('BA_Acknowledgement')."'";
$res = $db->query($sql);
if (!$res->numRows()) {
    $db->query("INSERT INTO `giv_components_template` (`name`, `ds_order`, `ds_name`, `ds_color_line`, `ds_color_area`, `ds_filled`, `ds_max`, `ds_min`, `ds_average`, `ds_last`, `ds_tickness`, `ds_transparency`, `ds_invert`, `default_tpl1`, `comment`)
                VALUES ('BA_Acknowledgement', NULL, 'BA_Acknowledgement', '#E76F17', '#F7D0B4', '1', '1', '1', '1', '1', 1, '50', NULL, NULL, NULL)");
}
