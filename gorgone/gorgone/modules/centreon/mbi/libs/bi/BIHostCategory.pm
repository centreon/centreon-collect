# 
# Copyright 2019 Centreon (http://www.centreon.com/)
#
# Centreon is a full-fledged industry-strength solution that meets
# the needs in IT infrastructure and application monitoring for
# service performance.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

use strict;
use warnings;

package gorgone::modules::centreon::mbi::libs::bi::BIHostCategory;

# Constructor
# parameters:
# $logger: instance of class CentreonLogger
# $centreon: Instance of centreonDB class for connection to Centreon database
# $centstorage: (optionnal) Instance of centreonDB class for connection to Centstorage database
sub new {
	my $class = shift;
	my $self  = {};
	$self->{"logger"}	= shift;
	$self->{"centstorage"} = shift;
	if (@_) {
		$self->{"centreon"}  = shift;
	}
	bless $self, $class;
	return $self;
}

sub getAllEntries {
	my $self = shift;
	my $db = $self->{"centstorage"};

	my $query = "SELECT `hc_id`, `hc_name`";
	$query .= " FROM `mod_bi_hostcategories`";
	my $sth = $db->query({ query => $query });
	my @entries = ();
	while (my $row = $sth->fetchrow_hashref()) {
		push @entries, [ $row->{"hc_id"}, $row->{"hc_name"} ];
	}
	$sth->finish();
	return (\@entries);
}

sub getEntryIds {
	my $self = shift;
	my $db = $self->{"centstorage"};

	my $query = "SELECT `id`, `hc_id`, `hc_name`";
	$query .= " FROM `mod_bi_hostcategories`";
	my $sth = $db->query({ query => $query });
	my %entries = ();
	while (my $row = $sth->fetchrow_hashref()) {
		$entries{$row->{"hc_id"}.";".$row->{"hc_name"}} = $row->{"id"};
	}
	$sth->finish();
	return (\%entries);
}

sub entryExists {
	my $self = shift;
	my ($value, $entries) = (shift, shift);
	foreach my $entry (@$entries) {
		my $diff = grep { $entry->[$_] ne $value->[$_] } 0..$#$entry;
		return 1 unless $diff;
	}
	return 0;
}
sub insert {
	my $self = shift;
	my $db = $self->{"centstorage"};
	my $logger =  $self->{"logger"};
	my $data = shift;
	my $query = "INSERT INTO `mod_bi_hostcategories`".
				" (`hc_id`, `hc_name`)".
				" VALUES (?,?)";
	my $sth = $db->prepare($query);	
	my $inst = $db->getInstance;
	$inst->begin_work;
	my $counter = 0;
	
	my $existingEntries = $self->getAllEntries;
	foreach my $entry (@$data) {
		if (!$self->entryExists($entry, $existingEntries)) {
			my ($hc_id, $hc_name) = @$entry;
			$sth->bind_param(1, $hc_id);
			$sth->bind_param(2, $hc_name);
			$sth->execute;
			if (defined($inst->errstr)) {
		  		$logger->writeLog("FATAL", "hostcategories insertion execute error : ".$inst->errstr);
			}
			if ($counter >= 1000) {
				$counter = 0;
				$inst->commit;
				if (defined($inst->errstr)) {
		  			$logger->writeLog("FATAL", "hostcategories insertion commit error : ".$inst->errstr);
				}
				$inst->begin_work;
			}
			$counter++;
		}
	}
	$inst->commit;
}

sub truncateTable {
	my $self = shift;
	my $db = $self->{"centstorage"};
	
	my $query = "TRUNCATE TABLE `mod_bi_hostcategories`";
	$db->query({ query => $query });
	$db->query({ query => "ALTER TABLE `mod_bi_hostcategories` AUTO_INCREMENT=1" });
}

1;
