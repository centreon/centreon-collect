
use strict;
use warnings;

package centreon::reporting::CentreonDownTime;

# Constructor
# parameters:
# $logger: instance of class CentreonLogger
# $centreon: Instance of centreonDB class for connection to Centreon database
# $dbLayer : Database Layer : ndo | broker
# $centstorage: (optionnal) Instance of centreonDB class for connection to Centstorage database
sub new {
    my $class = shift;
    my $self  = {};
    $self->{"logger"}    = shift;
    $self->{"centstatus"} = shift;
    $self->{'dbLayer'} = shift;
    if (@_) {
        $self->{"centstorage"}  = shift;
    }    
    bless $self, $class;
    return $self;
}

# returns two references to two hash tables => hosts indexed by id and hosts indexed by name
sub getDownTime {
    my $self = shift;
    my $centreon = $self->{"centstatus"};
    my $allIds = shift;
    my $start = shift;
    my $end = shift;
    my $type = shift; # if 1 => host, if 2 => service
    my $dbLayer = $self->{'dbLayer'};
    my $query;
    
    if ($dbLayer eq "ndo") {
        $query = "SELECT `name1`, `name2`,".
                " UNIX_TIMESTAMP(`scheduled_start_time`) as start_time,".
                " UNIX_TIMESTAMP(`actual_end_time`) as end_time".
            " FROM `nagios_downtimehistory` d, `nagios_objects` o".
            " WHERE o.`object_id` = d.`object_id` AND o.`objecttype_id` = '".$type."'".
            " AND was_started = 1".
			" AND `scheduled_start_time` < FROM_UNIXTIME(".$end.")".
			" AND (`actual_end_time` > FROM_UNIXTIME(".$start.") || `actual_end_time` = '0000-00-00')".
            " ORDER BY `name1` ASC, `scheduled_start_time` ASC, `actual_end_time` ASC";
    } elsif ($dbLayer eq "broker") {
        $query = "SELECT DISTINCT h.name as name1, s.description as name2, " .
                 "d.start_time, d.end_time " .
                 "FROM `hosts` h, `downtimes` d " .
                 "LEFT JOIN services s ON s.service_id = d.service_id " .
                 "WHERE started = 1 " .
                 "AND d.host_id = h.host_id ";
        if ($type == 1) {
            $query .= "AND d.type = 2 "; # That can be confusing, but downtime_type 2 is for host
        } elsif ($type == 2) {
            $query .= "AND d.type = 1 "; # That can be confusing, but downtime_type 1 is for service
        }
        $query .= "AND start_time < " . $end . " " .
                 "AND (end_time > " . $start . " || end_time = 0) " .
                 "ORDER BY name1 ASC, start_time ASC, end_time ASC";        
    }

    my ($status, $sth) = $centreon->query($query);
    
    my @periods = ();
    while (my $row = $sth->fetchrow_hashref()) {
        my $id = $row->{"name1"};
        if ($type == 2) {
            $id .= ";;".$row->{"name2"}
        }
        if (defined($allIds->{$id})) {
            if ($row->{"start_time"} < $start) {
                $row->{"start_time"} = $start;
            }
            if ($row->{"end_time"} > $end || $row->{"end_time"} == 0) {
                $row->{"end_time"} = $end;
            }
            
            my $insert = 1;
            for (my $i = 0; $i < scalar(@periods) && $insert; $i++) {
                my $checkTab = $periods[$i];
                if ($checkTab->[0] eq $allIds->{$id}){
                    if ($row->{"start_time"} <= $checkTab->[2] && $row->{"end_time"} <= $checkTab->[2]) {
                        $insert = 0;
                    } elsif ($row->{"start_time"} <= $checkTab->[2] && $row->{"end_time"} > $checkTab->[2]) {
                        $checkTab->[2] = $row->{"end_time"};
                        $periods[$i] = $checkTab;
                        $insert = 0;
                    }
                }
            }
            if ($insert) {
                my @tab = ($allIds->{$id}, $row->{"start_time"}, $row->{"end_time"});
                $periods[scalar(@periods)] = \@tab;
            }
        }
    }
    $sth->finish();
    return (\@periods);
}

sub splitInsertEventDownTime {
    my $self = shift;
    
    my $objectId = shift;
    my $start = shift;
    my $end = shift;
    my $downTimes = shift;
    my $state = shift;
    
    my @events = ();
    my $total = 0;
    if ($state ne "" && defined($downTimes) && defined($state) && $state != 0) {
        $total = scalar(@$downTimes);
    }
    for (my $i = 0; $i < $total && $start < $end; $i++) {
         my $tab = $downTimes->[$i];
         my $id = $tab->[0];
         my $downTimeStart = $tab->[1];
         my $downTimeEnd = $tab->[2];
         
         if ($id eq $objectId) {
             if ($downTimeStart < $start) {
                 $downTimeStart = $start;
             }
             if ($downTimeEnd > $end) {
                 $downTimeEnd = $end;
             }
             if ($downTimeStart < $end && $downTimeEnd > $start) {
                 if ($downTimeStart > $start) {
                     my @tab = ($start, $downTimeStart, 0);
                     $events[scalar(@events)] = \@tab;
                 }
                 my @tab = ($downTimeStart, $downTimeEnd, 1);
                 $events[scalar(@events)] = \@tab;
                 $start = $downTimeEnd;
             }
         }
    }
    if ($start < $end) {
        my @tab = ($start, $end, 0);
        $events[scalar(@events)] = \@tab;
    }
    return (\@events);
}

sub splitUpdateEventDownTime {
    my $self = shift;
    
    my $objectId = shift;
    my $start = shift;
    my $end = shift;
    my $downTimeFlag = shift;
    my $downTimes = shift;
    my $state = shift;
    
    my $updated = 0;
    my @events = ();
    my $updateTime = 0;
    my $total = 0;
    if (defined($downTimes) && $state != 0) {
        $total = scalar(@$downTimes);
    }
    for (my $i = 0; $i <  $total && $start < $end; $i++) {
        my $tab = $downTimes->[$i];
         my $id = $tab->[0];
         my $downTimeStart = $tab->[1];
         my $downTimeEnd = $tab->[2];
 
         if ($id eq $objectId) {
             if ($downTimeStart < $start) {
                 $downTimeStart = $start;
             }
             if ($downTimeEnd > $end) {
                 $downTimeEnd = $end;
             }
             if ($downTimeStart < $end && $downTimeEnd > $start) {
                 if ($updated == 0) {
                    $updated = 1;
                    if ($downTimeStart > $start) {
                        if ($downTimeFlag == 1) {
                            my @tab = ($start, $downTimeStart, 0);
                            $events[scalar(@events)] = \@tab;
                        }else {
                            $updateTime = $downTimeStart;
                        }
                        my @tab = ($downTimeStart, $downTimeEnd, 1);
                        $events[scalar(@events)] = \@tab;
                    }else {
                        if ($downTimeFlag == 1) {
                            $updateTime = $downTimeEnd;
                        }else {
                            my @tab = ($downTimeStart, $downTimeEnd, 1);
                            $events[scalar(@events)] = \@tab;
                        }            
                    }
                }else {
                    if ($downTimeStart > $start) {
                        my @tab = ($start, $downTimeStart, 0);
                        $events[scalar(@events)] = \@tab;
                    }
                    my @tab = ($downTimeStart, $downTimeEnd, 1);
                    $events[scalar(@events)] = \@tab;
                }
                $start = $downTimeEnd;
            }
         }
    }
    if ($start < $end && scalar(@events)) {
        my @tab = ($start, $end, 0);
        $events[scalar(@events)] = \@tab;
    } else {
        $updateTime = $end;
        if (scalar(@events) && $end > $events[0][0]) {
            $updateTime = $events[0][0];
        }
    }
    return ($updateTime, \@events);
}

1;