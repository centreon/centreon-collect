DELETE mod_bam_kpi FROM mod_bam_kpi
LEFT OUTER JOIN (
        SELECT MIN(kpi_id) as kpi_id, host_id, service_id, id_ba, meta_id, boolean_id
        FROM mod_bam_kpi
        GROUP BY host_id, service_id, id_ba, meta_id, boolean_id
    ) as t1
    ON mod_bam_kpi.kpi_id = t1.kpi_id
WHERE t1.kpi_id IS NULL;
