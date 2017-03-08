<?php
/*
 * MERETHIS
 *
 * Source Copyright 2005-2014 MERETHIS
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more information : contact@merethis.com
 *
 */

/* init $pearDBStorage if var is not defined */
if (!isset($pearDBStorage)) {
	require_once $centreon_path . 'www/modules/centreon-bam-server/core/class/Centreon/Db.php';
	$pearDBStorage = new CentreonBam_Db("centstorage");
}

/* Fix the BA events - KPI events relation */
/* Get all the kpi events without relation */
$query = "SELECT kpi_event_id, kpi_id, start_time"
       . "  FROM mod_bam_reporting_kpi_events AS kpi"
       . "  WHERE NOT EXISTS"
       . "    (SELECT 1 FROM mod_bam_reporting_relations_ba_kpi_events AS rel"
       . "       WHERE kpi.kpi_event_id = rel.kpi_event_id)";
$stmt = $pearDBStorage->query($query);
if (PEAR::isError($stmt))
  throw new Exception($stmt->getMessage());

/* For every kpi event without relation, insert the relation manually */
while ($stmt->fetchInto($row)) {
  $query2 = "INSERT INTO mod_bam_reporting_relations_ba_kpi_events"
          . "           (ba_event_id, kpi_event_id)"
          . "  SELECT be.ba_event_id, ke.kpi_event_id"
          . "    FROM mod_bam_reporting_kpi_events AS ke"
          . "    INNER JOIN mod_bam_reporting_ba_events AS be"
          . "    ON ((ke.start_time >= be.start_time)"
          . "       AND (be.end_time IS NULL OR ke.start_time < be.end_time))"
          . "    INNER JOIN mod_bam_reporting_kpi AS rki"
          . "     ON (rki.ba_id = be.ba_id AND rki.kpi_id = ke.kpi_id)"
          . "    WHERE ke.kpi_id=" .$row['kpi_id']. " AND ke.start_time=" .$row['start_time'];
  $kpiQuery = $pearDBStorage->query($query2);
}

