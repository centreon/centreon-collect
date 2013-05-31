
use strict;
use warnings;

package centreon::reporting::CentreonHostStateEvents;

# Constructor
# parameters:
# $logger: instance of class CentreonLogger
# $centreon: Instance of centreonDB class for connection to Centreon database
# $centstorage: (optionnal) Instance of centreonDB class for connection to Centstorage database
sub new {
    my $class = shift;
    my $self  = {};
    $self->{"logger"} = shift;
    $self->{"centstorage"} = shift;
    $self->{"centreonAck"} = shift;
    $self->{"centreonDownTime"} = shift;
    bless $self, $class;
    return $self;
}

# Get events in given period
# Parameters:
# $start: period start
# $end: period end
sub getStateEventDurations {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    my $start = shift;
    my $end = shift;
    my %hosts;
    my $query = "SELECT `host_id`, `state`,  `start_time`, `end_time`, `in_downtime`".
                " FROM `hoststateevents`".
                " WHERE `start_time` < ".$end.
                    " AND `end_time` > ".$start.
                    " AND `state` < 3"; # STATE PENDING AND UNKNOWN NOT HANDLED
    my $sth = $centstorage->query($query);
    while (my $row = $sth->fetchrow_hashref()) {
        if ($row->{"start_time"} < $start) {
            $row->{"start_time"} = $start;
        }
        if ($row->{"end_time"} > $end) {
            $row->{"end_time"} = $end;
        }
        if (!defined($hosts{$row->{"host_id"}})) {
            my @tab = (0, 0, 0, 0, 0, 0, 0, 0);
            # index 0: UP, index 1: DOWN, index 2: UNREACHABLE, index 3: DOWNTIME, index 4: UNDETERMINED
            # index 5: UP alerts, index 6: Down alerts, , index 7: Unreachable alerts
            $hosts{$row->{"host_id"}} = \@tab;            
        }
        
        my $stats = $hosts{$row->{"host_id"}};
        if ($row->{"in_downtime"} == 0) {
            $stats->[$row->{"state"}] += $row->{"end_time"} - $row->{"start_time"};
            $stats->[$row->{"state"} + 5] += 1;
        }else {
            $stats->[3] += $row->{"end_time"} - $row->{"start_time"};
        }
        $hosts{$row->{"host_id"}} = $stats;
    }
    my %results;
    while (my ($key, $value) = each %hosts) {
        $value->[4] = ($end - $start) - ($value->[0] + $value->[1] + $value->[2] + $value->[3]);
        $results{$key} = $value;
    }
    return (\%results);
}

# Get last events for each host
# Parameters:
# $start: max date possible for each event
# $serviceNames: references a hash table containing a list of host
sub getLastStates {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    my $hostNames = shift;
    
    my %currentStates;
    
    my $query = "SELECT `host_id`, `state`, `hoststateevent_id`, `end_time`, `in_downtime`".
                " FROM `hoststateevents`".
                " WHERE `last_update` = 1";
    my $sth = $centstorage->query($query);
    while(my $row = $sth->fetchrow_hashref()) {
        if (defined($hostNames->{$row->{'host_id'}})) {
            my @tab = ($row->{'end_time'}, $row->{'state'}, $row->{'hoststateevent_id'}, $row->{'in_downtime'});
            $currentStates{$hostNames->{$row->{'host_id'}}} = \@tab;
        }
    }
    $sth->finish();
    
    return (\%currentStates);
}

# update a specific host incident end time
# Parameters
# $endTime: incident end time
# $eventId: ID of event to update
sub updateEventEndTime {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    my $centreonDownTime = $self->{"centreonDownTime"};
    my $centreonAck = $self->{"centreonAck"};
    
    my ($id, $hostId, $start, $end, $state, $eventId, $downTimeFlag, $lastUpdate, $downTime) = (shift, shift, shift, shift, shift, shift, shift, shift, shift);

    my ($events, $updateTime);
    ($updateTime, $events) = $centreonDownTime->splitUpdateEventDownTime($hostId, $start, $end, $downTimeFlag,$downTime, $state);

    my $totalEvents = 0;
    if (defined($events)) {
        $totalEvents = scalar(@$events);
    }
    my $ack = $centreonAck->getHostAckTime($start, $updateTime, $id);
    if (!$totalEvents && $updateTime) {
        my $query = "UPDATE `hoststateevents` SET `end_time` = ".$updateTime.", `ack_time`=".$ack.",  `last_update`=".$lastUpdate.
                    " WHERE `hoststateevent_id` = ".$eventId;
        $centstorage->query($query);
    }else {
        if ($updateTime) {
            my $query = "UPDATE `hoststateevents` SET `end_time` = ".$updateTime.", `ack_time`=".$ack.",  `last_update`= 0".
                    " WHERE `hoststateevent_id` = ".$eventId;
            $centstorage->query($query);
        }
        $self->insertEventTable($id, $hostId, $state, $lastUpdate, $events);
    }
}

# insert a new incident for host
# Parameters
# $hostId : host ID
# $serviceId: service ID
# $state: incident state
# $start: incident start time
# $end: incident end time
sub insertEvent {
    my $self = shift;
    my $centreonDownTime = $self->{"centreonDownTime"};
    
    my ($id, $hostId, $state, $start, $end, $lastUpdate, $downTime) = (shift, shift, shift, shift, shift, shift, shift);
    
    my $events = $centreonDownTime->splitInsertEventDownTime($hostId, $start, $end, $downTime, $state);
    if ($state ne "") {
        $self->insertEventTable($id, $hostId, $state, $lastUpdate, $events);
    }
}

sub insertEventTable {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    my $centreonAck = $self->{"centreonAck"};
    
    my ($id, $hostId, $state, $lastUpdate, $events) =  (shift, shift, shift, shift, shift);
    
    my $query_start = "INSERT INTO `hoststateevents`".
            " (`host_id`, `state`, `start_time`, `end_time`, `last_update`, `in_downtime`, `ack_time`)".
            " VALUES (";
    my $count = 0;
    my $totalEvents = 0;

    for($count = 0; $count < scalar(@$events) - 1; $count++) {
        my $tab = $events->[$count];
        my $ack = $centreonAck->getHostAckTime($tab->[0], $tab->[1], $id);
        my $query_end = $hostId.", ".$state.", ".$tab->[0].", ".$tab->[1].", 0, ".$tab->[2].", ".$ack.")";
        $centstorage->query($query_start.$query_end);
    }
    if (scalar(@$events)) {
        my $tab = $events->[$count];
        if (defined($hostId) && defined($state)) {
            my $ack = $centreonAck->getHostAckTime($tab->[0], $tab->[1], $id);
            my $query_end = $hostId.", ".$state.", ".$tab->[0].", ".$tab->[1].", ".$lastUpdate.", ".$tab->[2].", ".$ack.")";        
            $centstorage->query($query_start.$query_end);
        }
    }
}

# Truncate service incident table
sub truncateStateEvents {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    
    my $query = "TRUNCATE TABLE `hoststateevents`";
    $centstorage->query($query);
}

# Get first and last events date
sub getFirstLastIncidentTimes {
    my $self = shift;
    my $centstorage = $self->{"centstorage"};
    
    my $query = "SELECT min(`start_time`) as minc, max(`end_time`) as maxc FROM `hoststateevents`";
    my $sth = $centstorage->query($query);
    my ($start, $end) = (0,0);
    if (my $row = $sth->fetchrow_hashref()) {
        ($start, $end) = ($row->{"minc"}, $row->{"maxc"});
    }
    $sth->finish;
    return ($start, $end);
}

1;