#! /usr/bin/perl -w
####################################################################
####################################################################

use strict;
use Term::ANSIColor;

####################################################################
# Required libs
####################################################################
use DBI;
use POSIX;
use Getopt::Long;
use Time::Local;

####################################################################
# Global Variables
####################################################################

# Include Centreon DB Configuration Variables
use vars qw ($mysql_database_oreon $mysql_database_ods $mysql_host $mysql_user $mysql_passwd);
require "@CENTREON_ETC@/conf.pm";

# DB Connection instance
my $db;
my $db_storage;

my %options;

my %statusCode = (
	"OK" 		=> 0,
	"WARNING" 	=> 1,
	"CRITICAL" 	=> 2,
	"UNKNOWN"	=> 3
);

####################################################################
#FUNCTIONS
####################################################################

#################################
# Print ok message
#################################
sub print_ok {
	print color 'bold green';
	print "OK\n";
	print color 'reset';
}


#################################
# Program execution help function
#################################
sub print_help {
	print "This program is for migrating BA data from version 2.x to 3.x\n";
	print "Usage: ".$0." [-h|--help]\n\n";
	print "Options:\n";
	print "--help         Prints this help message\n";
	exit;
}

##############################
#
##############################
sub log_info($) {
	my ($info) = shift;
	#print "$info\n";
}

#################################
# Handle options
#################################
sub handle_options {
	Getopt::Long::Configure('bundling');
	GetOptions(
		"h"   => \$options{"help"}, "help"  => \$options{"help"}
	);
	
	if ($options{"help"}) {
		print_help;
	}
}

#################################
# Synchronizing real time data
#################################
sub syncRealTimeData {
	my ($sql, $stmt, $stmt2, $stmt3);

    # sync ba
	$sql = "SELECT SUBSTRING_INDEX(description, '_', -1) as ba_id, last_hard_state, last_hard_state_change FROM services WHERE description LIKE 'ba\_%'";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;

	while (my ($ba_id, $last_hard_state, $last_hard_state_change) = $stmt->fetchrow_array()) {
		$stmt2 = $db->prepare("UPDATE mod_bam SET current_status = ?, last_state_change = UNIX_TIMESTAMP(NOW()) WHERE ba_id =  ?");
		$stmt2->execute(($last_hard_state, $ba_id))
			or log_info "Error: " . $db->errstr;

		$stmt2 = $db->prepare("UPDATE mod_bam_kpi SET current_status = ?, last_state_change = UNIX_TIMESTAMP(NOW()) WHERE id_indicator_ba = ?");
		$stmt2->execute(($last_hard_state, $ba_id))
			or log_info "Error: " . $db->errstr;
	}

	# sync service kpi
	$sql = "SELECT host_id, service_id FROM mod_bam_kpi WHERE host_id IS NOT NULL AND service_id IS NOT NULL";
	$stmt = $db->prepare($sql);
	$stmt->execute()
		or log_info "Error: " . $db->errstr;

	while (my ($host_id, $service_id) = $stmt->fetchrow_array()) {
		$stmt2 = $db_storage->prepare("SELECT last_hard_state, last_hard_state_change FROM services WHERE host_id = ? AND service_id = ?");
		$stmt2->execute(($host_id, $service_id))
			or log_info "Error: " . $db_storage->errstr;
		while (my($last_hard_state, $last_hard_state_change) = $stmt2->fetchrow_array()) {
			$stmt3 = $db->prepare("UPDATE mod_bam_kpi SET current_status = ?, last_state_change = UNIX_TIMESTAMP(NOW()) WHERE host_id = ? AND service_id = ?");
			$stmt3->execute(($last_hard_state, $host_id, $service_id))
				or log_info "Error: " . $db->errstr;
		}
	}

	# sync meta kpi
	$sql = "SELECT SUBSTRING_INDEX(description, '_', -1) as meta_id, last_hard_state, last_hard_state_change FROM services WHERE description LIKE 'meta\_%'";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;

	while (my ($meta_id, $last_hard_state, $last_hard_state_change) = $stmt->fetchrow_array()) {
		$stmt2 = $db->prepare("UPDATE mod_bam_kpi SET current_status = ?, last_state_change = UNIX_TIMESTAMP(NOW()) WHERE meta_id =  ?");
		$stmt2->execute(($last_hard_state, $meta_id));
	}
}

#################################
# Purge ba kpi
#################################
sub purgeBaKpi {
  my $sql;
  my $stmt;

  $sql = "DELETE FROM mod_bam_reporting_relations_ba_kpi_events";
  $stmt = $db_storage->prepare($sql);
  $stmt->execute()
    or log_info "Error: " . $db_storage->errstr;

  $sql = "DELETE FROM mod_bam_reporting_kpi_events";
  $stmt = $db_storage->prepare($sql);
  $stmt->execute()
    or log_info "Error: " . $db_storage->errstr;

  $sql = "DELETE FROM mod_bam_reporting_ba_events";
  $stmt = $db_storage->prepare($sql);
  $stmt->execute()
    or log_info "Error: " . $db_storage->errstr;

  $sql = "DELETE FROM mod_bam_reporting_ba_events_durations";
  $stmt = $db_storage->prepare($sql);
  $stmt->execute()
    or log_info "Error: " . $db_storage->errstr;

  $sql = "DELETE FROM mod_bam_reporting_ba_availabilities";
  $stmt = $db_storage->prepare($sql);
  $stmt->execute()
    or log_info "Error: " . $db_storage->errstr;
}


#################################
# Migrate ba logs
#################################
sub migrateBaLogs {
	my ($sql, $stmt, $insertSql, $insertStmt, $endSql, $endStmt, $flagStmt);
	my $previousBaEventId = 0;
	my $previousBaId = 0;
	my $previousStatus = -1;
	my $previousInDowntime = 0;
	my $previousStateChange = 0;
	my $status;

	# Retrieve logs
	$sql = "SELECT l.ba_id, l.level, l.in_downtime, l.status, l.status_change_flag, l.downtime_flag, l.ctime
		FROM mod_bam_logs l, mod_bam_reporting_ba b
		WHERE l.ba_id = b.ba_id
		ORDER BY l.ba_id, l.ctime ASC";

	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;

	# sql queries
	$endSql = "UPDATE mod_bam_reporting_ba_events SET end_time = ? WHERE ba_event_id = ?";
	$insertSql = "INSERT INTO mod_bam_reporting_ba_events (ba_id, start_time, status, first_level, in_downtime) VALUES (?, ?, ?, ?, ?)";

	while (my ($baId, $level, $inDowntime, $statusString, $statusChangeFlag, $downtimeFlag, $startTime) = $stmt->fetchrow_array()) {
		# get status
		$status = $statusCode{$statusString};
		if ($statusChangeFlag || $downtimeFlag) {
			# put an end to the last event
			if ($previousBaEventId && ($previousBaId == $baId)) {
				$endStmt = $db_storage->prepare($endSql); 
				$endStmt->execute(($startTime, $previousBaEventId)) 
					or log_info "Error: " . $db_storage->errstr;
			}

			# we previous kpi event hasn't ended, we need to flag that
			if ($previousBaId && $baId != $previousBaId && $previousStatus != -1) {
				$flagStmt = $db->prepare(
					"UPDATE mod_bam 
					SET last_state_change = ?, current_status = ?, in_downtime = ?
					WHERE ba_id = ?"
				);
				$flagStmt->execute(($previousStateChange, $previousStatus, $previousInDowntime, $previousBaId))
					or log_info "Error: " . $db->errstr;
			}	

			# insert the new event
			$insertStmt = $db_storage->prepare($insertSql);
			$insertStmt->execute(($baId, $startTime, $status, $level, $inDowntime))
				or log_info "Error: " . $db_storage->errstr;

			# retrieve last id
			$previousBaEventId = $insertStmt->{mysql_insertid};
			$previousBaId = $baId;
			$previousStatus = $status;
			$previousInDowntime = $inDowntime;
			$previousStateChange = $startTime;
		}
	}

	# handle the very last BA
	if ($previousBaId) {
		$flagStmt = $db->prepare(
			"UPDATE mod_bam 
			SET last_state_change = ?, current_status = ?, in_downtime = ?
			WHERE ba_id = ?"
		);
		$flagStmt->execute(($previousStateChange, $previousStatus, $previousInDowntime, $previousBaId))
			or log_info "Error: " . $db->errstr;
	}
}


#################################
# Migrate kpi logs
#################################
sub migrateKpiLogs {
	my ($sql, $stmt, $insertSql, $insertStmt, $endSql, $endStmt, $relSql, $relStmt, $flagStmt);
	my $previousKpiEventId = 0;
	my $previousKpiId = 0;
	my $previousStatus = -1;
	my $previousInDowntime = 0;
	my $previousStateChange = 0;
	my $status;

	# Retrieve logs
	$sql = "SELECT kpi.kpi_id, kpi.ba_id, kpi.status, output, kpi.in_downtime, kpi.downtime_flag, ctime, ba_event_id, impact
		FROM mod_bam_kpi_logs kpi, mod_bam_reporting_ba_events ba, mod_bam_reporting_kpi r
		WHERE kpi.ba_id = ba.ba_id
		AND ba.start_time = kpi.ctime
		AND kpi.kpi_id = r.kpi_id
		ORDER BY kpi_id, ctime ASC";

	$stmt = $db_storage->prepare($sql);
	$stmt->execute()
		or log_info "Error:" . $db_storage->errstr;

	# sql queries	
	$relSql = "INSERT INTO mod_bam_reporting_relations_ba_kpi_events (ba_event_id, kpi_event_id) VALUES (?, ?)";
	$endSql = "UPDATE mod_bam_reporting_kpi_events SET end_time = ? WHERE kpi_event_id = ?";
	$insertSql = "INSERT INTO mod_bam_reporting_kpi_events (kpi_id, start_time, status, impact_level, in_downtime, first_output) VALUES (?, ?, ?, ?, ?, ?)";

	while (my ($kpiId, $baId, $status, $output, $inDowntime, $downtimeFlag, $startTime, $baEventId, $impact) = $stmt->fetchrow_array()) {
		if (($kpiId == $previousKpiId && $status != $previousStatus) || $downtimeFlag || $kpiId != $previousKpiId) {
			# put an end to the last event
			if ($previousKpiEventId && ($kpiId == $previousKpiId)) {
				$endStmt = $db_storage->prepare($endSql); 
				$endStmt->execute(($startTime, $previousKpiEventId))
					or log_info "Error: " . $db_storage->errstr;
			}

			# we previous kpi event hasn't ended, we need to flag that
			if ($previousKpiId && $kpiId != $previousKpiId && $previousStatus != -1) {
				$flagStmt = $db->prepare(
					"UPDATE mod_bam_kpi 
					SET last_state_change = ?, current_status = ?, in_downtime = ?
					WHERE kpi_id = ?"
				);
				$flagStmt->execute(($previousStateChange, $previousStatus, $previousInDowntime, $previousKpiId))
					or log_info "Error: " . $db->errstr;
			}

			# insert the new event
			$insertStmt = $db_storage->prepare($insertSql);
			$insertStmt->execute(($kpiId, $startTime, $status, $impact, $inDowntime, $output))
				or log_info "Error: " . $db_storage->errstr;

			# retrieve last id and last status
			$previousKpiEventId = $insertStmt->{mysql_insertid};
			$previousStatus = $status;
			$previousKpiId = $kpiId;
			$previousInDowntime = $inDowntime;
			$previousStateChange = $startTime;

			# insert relations
			$relStmt = $db_storage->prepare($relSql);
			$relStmt->execute(($baEventId, $previousKpiEventId))
				or log_info "Error: " . $db_storage->errstr;
		}
	}

	# handle the very last kpi
	if ($previousKpiId) {
		$flagStmt = $db->prepare(
			"UPDATE mod_bam_kpi 
			SET last_state_change = ?, current_status = ?, in_downtime = ?
			WHERE kpi_id = ?"
		);
		$flagStmt->execute(($previousStateChange, $previousStatus, $previousInDowntime, $previousKpiId))
			or log_info "Error: " . $db->errstr;
	}
}

#################################
# Insert reporting data
#################################
sub insertReportingData {
	my $insertSql;
	my $insertStmt;
	my $stmt;

	# dimension data for business activies
	$insertSql = "INSERT INTO mod_bam_reporting_ba (ba_id, ba_name, ba_description) VALUES (?, ?, ?)";

	$stmt = $db->prepare("SELECT ba_id, name, description FROM mod_bam");
	$stmt->execute() or log_info "Error: " . $db->errstr;
	while (my($baId, $baName, $baDescription) = $stmt->fetchrow_array()) {
		$insertStmt = $db_storage->prepare($insertSql);
		$insertStmt->execute(($baId, $baName, $baDescription))
			or log_info "Error: " . $db_storage->errstr;
	}

	# dimension data for regular kpi
	$insertSql = "INSERT INTO mod_bam_reporting_kpi 
		(kpi_id, ba_id, host_id, service_id, meta_service_id, boolean_id, kpi_ba_id) 
		VALUES (?, ?, ?, ?, ?, ?, ?)";

	$stmt = $db->prepare("
		SELECT kpi_id, id_ba, host_id, service_id, meta_id, boolean_id, id_indicator_ba
		FROM mod_bam_kpi");
	$stmt->execute() or log_info "Error: " . $db->errstr;
	while (my($kpiId, $baId, $hostId, $serviceId, $metaId, $boolId, $kpiBaId) = $stmt->fetchrow_array()) {
		$insertStmt = $db_storage->prepare($insertSql);
		$insertStmt->execute(($kpiId, $baId, $hostId, $serviceId, $metaId, $boolId, $kpiBaId))
			or log_info "Error: " . $db_storage->errstr;
	}
}

#################################
# Purge corrupted events
#################################
sub purgeCorruptedEvents {
	my $sql;
	my $stmt;

	$sql = "DELETE FROM mod_bam_reporting_kpi_events WHERE start_time = end_time";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;

	$sql = "DELETE FROM mod_bam_reporting_ba_events WHERE start_time = end_time";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;

	$sql = "UPDATE mod_bam_reporting_kpi_events SET end_time = UNIX_TIMESTAMP(NOW()) WHERE end_time IS NULL";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;	

	$sql = "UPDATE mod_bam_reporting_ba_events SET end_time = UNIX_TIMESTAMP(NOW()) WHERE end_time IS NULL";
	$stmt = $db_storage->prepare($sql);
	$stmt->execute() 
		or log_info "Error: " . $db_storage->errstr;
}

################################################################

############################################################################################################
# Main function
#############################################################################################################

sub main {
	handle_options;

	# Initializing MySQL DB connection
	$db = DBI->connect("DBI:mysql:database=".$mysql_database_oreon.";host=".$mysql_host, $mysql_user, $mysql_passwd,
						{'RaiseError' => 0, 'PrintError' => 0, 'AutoCommit' => 1});
	$db_storage = DBI->connect("DBI:mysql:database=".$mysql_database_ods.";host=".$mysql_host, $mysql_user, $mysql_passwd,
						{'RaiseError' => 0, 'PrintError' => 0, 'AutoCommit' => 1});


	print "Stopping Centreon Broker... ";
	my $output = `/etc/init.d/cbd stop`;
	print_ok;

	print "Synchronizing dimension tables... ";
    insertReportingData;
	print_ok;
	
	print "Purging BA and KPI events...";
	purgeBaKpi;
	print_ok;

	print "Migrating BA events...";
	migrateBaLogs;
	print_ok;

	print "Migrating KPI events...";
	migrateKpiLogs;
	print_ok;

	print "Removing corrupted events...";
	purgeCorruptedEvents;
	print_ok;

	print "Synchronizing real time data...";
	syncRealTimeData;
	print_ok;

	$db->disconnect();
	$db_storage->disconnect();
}

#####################################################
# MAIN EXECUTION 
#####################################################
main();
