
use strict;
use warnings;

package centreon::reporting::CentreonHost;

# Constructor
# parameters:
# $logger: instance of class CentreonLogger
# $centreon: Instance of centreonDB class for connection to Centreon database
# $centstorage: (optionnal) Instance of centreonDB class for connection to Centstorage database
sub new {
    my $class = shift;
    my $self  = {};
    $self->{"logger"} = shift;
    $self->{"centreon"} = shift;
    if (@_) {
        $self->{"centstorage"}  = shift;
    }
    bless $self, $class;
    return $self;
}

# returns two references to two hash tables => hosts indexed by id and hosts indexed by name
sub getAllHosts {
    my $self = shift;
    my $centreon = $self->{"centreon"};
    my $activated = 1;
    if (@_) {
        $activated  = 0;
    }
    my (%host_ids, %host_names);
    
    my $query = "SELECT `host_id`, `host_name`".
                " FROM `host`".
                " WHERE `host_register`='1'";
    if ($activated == 1) {
        $query .= " AND `host_activate` ='1'";
    }
    my ($status, $sth) = $centreon->query($query);
    while (my $row = $sth->fetchrow_hashref()) {
        $host_ids{$row->{"host_name"}} = $row->{"host_id"};
        $host_names{$row->{"host_id"}} = $row->{"host_name"};
    }
    $sth->finish();
    return (\%host_ids,\%host_names);
}

# Get all hosts, keys are IDs
sub getAllHostsByID {
    my $self = shift;
    my ($host_ids, $host_names) = $self->getAllHosts();    
    return ($host_ids);
}

# Get all hosts, keys are names
sub getAllHostsByName {
    my $self = shift;
    my ($host_ids, $host_names) = $self->getAllHosts();    
    return ($host_names);
}

1;