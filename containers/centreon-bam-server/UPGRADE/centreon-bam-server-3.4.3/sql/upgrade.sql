ALTER TABLE mod_bam ADD inherit_kpi_downtimes BOOLEAN default 0;

CREATE VIEW mod_bam_view_kpi AS
SELECT k.kpi_id, b.ba_id, k.activate AS kpi_activate, b.activate AS ba_activate, b.name AS ba_name,
h.host_name AS kpi_host, s.service_description AS kpi_service, bk.name AS kpi_ba,
meta.meta_name AS kpi_meta, boo.name AS kpi_boolean, b.icon_id, k.kpi_type, k.config_type,
k.drop_warning, k.drop_critical, k.drop_unknown,
k.drop_warning_impact_id, k.drop_critical_impact_id, k.drop_unknown_impact_id,
b.level_w, b.level_c,
k.host_id, k.service_id, k.id_indicator_ba, k.meta_id, k.boolean_id
FROM mod_bam_kpi AS k
INNER JOIN mod_bam AS b ON b.ba_id = k.id_ba
LEFT JOIN host AS h ON h.host_id = k.host_id
LEFT JOIN service AS s ON s.service_id = k.service_id
LEFT JOIN mod_bam AS bk ON bk.ba_id = k.id_indicator_ba
LEFT JOIN meta_service AS meta ON meta.meta_id = k.meta_id
LEFT JOIN mod_bam_boolean as boo ON boo.boolean_id = k.boolean_id
ORDER BY b.name;

-- Avoid constraint error on boolean rule deletion
ALTER TABLE `mod_bam_kpi` DROP FOREIGN KEY `mod_bam_kpi_ibfk_8`;
ALTER TABLE `mod_bam_kpi` ADD CONSTRAINT `mod_bam_kpi_ibfk_8` FOREIGN KEY (`boolean_id`) REFERENCES `mod_bam_boolean` (`boolean_id`) ON DELETE CASCADE;

-- TOPOLOGY
INSERT INTO `topology` (`topology_name`, `topology_parent`, `topology_page`, `topology_order`, `topology_group`, `topology_url`, `topology_url_opt`, `topology_popup`, `topology_modules`, `topology_show`) VALUES 
('Dependencies', '626', '62612', '40', '1', './modules/centreon-bam-server/core/configuration/dependencies/configuration_dependencies.php', NULL, NULL, '1', '1');
