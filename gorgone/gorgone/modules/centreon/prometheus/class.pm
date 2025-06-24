# 
# Copyright 2025 Centreon (http://www.centreon.com/)
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

package gorgone::modules::centreon::prometheus::class;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::standard::constants qw(:all);
use gorgone::standard::misc;
use gorgone::class::sqlquery;
use gorgone::class::http::http;
use EV;
use Try::Tiny;

my %handlers = (TERM => {}, HUP => {});
my ($connector);

sub new {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{update_interval} = defined($connector->{config}->{update_interval}) && $connector->{config}->{update_interval} =~ /(\d+)/ ? $1 : 300;
    $connector->{update_last_time} = -1;

    $connector->{export_file} = 1;
    if (defined($connector->{config}->{export_file}) && $connector->{config}->{export_file} =~ /^(0|1|false|true)$/i) {
        $connector->{export_file} = $connector->{config}->{export_file} =~ /^(1|true)$/ ? 1 : 0;
    }
    $connector->{file} = defined($connector->{config}->{file}) && $connector->{config}->{file} =~ /\S/ ? $connector->{config}->{file} : 
        '/usr/share/centreon/www/exporters/prometheus';
    $connector->{tmp_file} = defined($connector->{config}->{tmp_file}) && $connector->{config}->{file} =~ /\S/ ? $connector->{config}->{tmp_file} : 
        $connector->{file} . '.tmp';

    $connector->{export_gateway} = 0;
    if (defined($connector->{config}->{export_gateway}) && $connector->{config}->{export_gateway} =~ /^(1|true)$/i) {
        $connector->{export_gateway} = 1;
    }
    $connector->{prometheus_gateway_address} = defined($connector->{config}->{prometheus_gateway_address}) && $connector->{config}->{prometheus_gateway_address} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_address} : 'http://localhost';
    $connector->{prometheus_gateway_port} = defined($connector->{config}->{prometheus_gateway_port}) && $connector->{config}->{prometheus_gateway_port} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_port} : 9091;
    $connector->{prometheus_gateway_job} = defined($connector->{config}->{prometheus_gateway_job}) && $connector->{config}->{prometheus_gateway_job} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_job} : 'monitoring';
    $connector->{prometheus_gateway_instance} = defined($connector->{config}->{prometheus_gateway_instance}) && $connector->{config}->{prometheus_gateway_instance} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_instance} : 'production';
    $connector->{prometheus_gateway_wipe_interval} = defined($connector->{config}->{prometheus_gateway_wipe_interval}) && $connector->{config}->{prometheus_gateway_wipe_interval} =~ /(\d+)/ ? 
        $connector->{config}->{prometheus_gateway_wipe_interval} : 86400;
    $connector->{prometheus_gateway_wipe_last_time} = -1;
    $connector->{prometheus_gateway_user} = defined($connector->{config}->{prometheus_gateway_user}) && $connector->{config}->{prometheus_gateway_user} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_user} : '';
    $connector->{prometheus_gateway_password} = defined($connector->{config}->{prometheus_gateway_password}) && $connector->{config}->{prometheus_gateway_password} =~ /\S/ ? 
        $connector->{config}->{prometheus_gateway_password} : '';
    $connector->{prometheus_gateway_insecure} = 0;
    if (defined($connector->{config}->{prometheus_gateway_insecure}) && $connector->{config}->{prometheus_gateway_insecure} =~ /^(1|true)$/i) {
        $connector->{prometheus_gateway_insecure} = 1;
    }

    $connector->{add_metrics} = 1;
    if (defined($connector->{config}->{add_metrics}) && $connector->{config}->{add_metrics} =~ /^(0|1|false|true)$/i) {
        $connector->{add_metrics} = $connector->{config}->{add_metrics} =~ /^(1|true)$/ ? 1 : 0;
    }
    $connector->{add_status} = 1;
    if (defined($connector->{config}->{add_status}) && $connector->{config}->{add_status} =~ /^(0|1|false|true)$/i) {
        $connector->{add_status} = $connector->{config}->{add_status} =~ /^(1|true)$/ ? 1 : 0;
    }
    $connector->{add_state} = 0;
    if (defined($connector->{config}->{add_state}) && $connector->{config}->{add_state} =~ /^(1|true)$/i) {
        $connector->{add_state} = 1;
    }
    $connector->{add_acknowledged} = 0;
    if (defined($connector->{config}->{add_acknowledged}) && $connector->{config}->{add_acknowledged} =~ /^(1|true)$/i) {
        $connector->{add_acknowledged} = 1;
    }

    $connector->{filter_hosts_from_hg_matching} = defined($connector->{config}->{filter_hosts_from_hg_matching}) && $connector->{config}->{filter_hosts_from_hg_matching} =~ /\S/ ? 
        $connector->{config}->{filter_hosts_from_hg_matching} : '';
    $connector->{exclude_hosts_from_hg_matching} = defined($connector->{config}->{exclude_hosts_from_hg_matching}) && $connector->{config}->{exclude_hosts_from_hg_matching} =~ /\S/ ? 
        $connector->{config}->{exclude_hosts_from_hg_matching} : '';
    $connector->{display_hg_matching} = defined($connector->{config}->{display_hg_matching}) && $connector->{config}->{display_hg_matching} =~ /\S/ ? 
        $connector->{config}->{display_hg_matching} : '';

    $connector->{host_state_type_metadata} = defined($connector->{config}->{host_state_type_metadata}) ? $connector->{config}->{host_state_type_metadata} : '# TYPE host_state gauge';
    $connector->{host_state_help_metadata} = defined($connector->{config}->{host_state_help_metadata}) ? $connector->{config}->{host_state_help_metadata} : '# HELP host_state 0 is SOFT, 1 is HARD';
    $connector->{host_state_template} = defined($connector->{config}->{host_state_template}) ? $connector->{config}->{host_state_template} : "host_state{host='%(host_name)'} %(host_state)";

    $connector->{host_status_type_metadata} = defined($connector->{config}->{host_status_type_metadata}) ? $connector->{config}->{host_status_type_metadata} : '# TYPE host_status gauge';
    $connector->{host_status_help_metadata} = defined($connector->{config}->{host_status_help_metadata}) ? $connector->{config}->{host_status_help_metadata} : '# HELP host_status 0 is UP, 1 is DOWN, 2 is UNREACHABLE, 4 is PENDING';
    $connector->{host_status_template} = defined($connector->{config}->{host_status_template}) ? $connector->{config}->{host_status_template} : 'host_status{host="%(host_name)"} %(host_status)';

    $connector->{service_state_type_metadata} = defined($connector->{config}->{service_state_type_metadata}) ? $connector->{config}->{service_state_type_metadata} : '# TYPE service_state gauge';
    $connector->{service_state_help_metadata} = defined($connector->{config}->{service_state_help_metadata}) ? $connector->{config}->{service_state_help_metadata} : '# HELP service_state 0 is SOFT, 1 is HARD';
    $connector->{service_state_template} = defined($connector->{config}->{service_state_template}) ? $connector->{config}->{service_state_template} : 'service_state{host="%(host_name)",service="%(service_description)"} %(service_state)';

    $connector->{service_ack_type_metadata} = defined($connector->{config}->{service_ack_type_metadata}) ? $connector->{config}->{service_ack_type_metadata} : '# TYPE service_ack gauge';
    $connector->{service_ack_help_metadata} = defined($connector->{config}->{service_ack_help_metadata}) ? $connector->{config}->{service_ack_help_metadata} : '# HELP service_ack 0 is unacknowledged, 1 is acknowledged';
    $connector->{service_ack_template} = defined($connector->{config}->{service_ack_template}) ? $connector->{config}->{service_ack_template} : 'service_ack{host="%(host_name)",service="%(service_description)"} %(service_acknowledged)';

    $connector->{service_status_type_metadata} = defined($connector->{config}->{service_status_type_metadata}) ? $connector->{config}->{service_status_type_metadata} : '# TYPE service_status gauge';
    $connector->{service_status_help_metadata} = defined($connector->{config}->{service_status_help_metadata}) ? $connector->{config}->{service_status_help_metadata} : '# HELP service_status is OK, 1 is WARNING, 2 is CRITICAL, 3 is UNKNOWN, 4 is PENDING';
    $connector->{service_status_template} = defined($connector->{config}->{service_status_template}) ? $connector->{config}->{service_status_template} : 'service_status{host="%(host_name)",service="%(service_description)"} %(service_status)';

    $connector->{add_metrics_metadata} = 1;
    if (defined($connector->{config}->{add_metrics_metadata}) && $connector->{config}->{add_metrics_metadata} =~ /^(0|1|false|true)$/i) {
        $connector->{add_metrics_metadata} = $connector->{config}->{add_metrics_metadata} =~ /^(1|true)$/ ? 1 : 0;
    }
    $connector->{metric_template} = defined($connector->{config}->{metric_template}) ? $connector->{config}->{metric_template} : '%(metric_name){host="%(host_name)",service="%(service_description)",dimensions="%(metric_dimensions)"} %(metric_value)';

    $connector->set_signal_handlers();
    return $connector;
}

sub set_signal_handlers {
    my $self = shift;

    $SIG{TERM} = \&class_handle_TERM;
    $handlers{TERM}->{$self} = sub { $self->handle_TERM() };
    $SIG{HUP} = \&class_handle_HUP;
    $handlers{HUP}->{$self} = sub { $self->handle_HUP() };
}

sub handle_HUP {
    my $self = shift;
    $self->{reload} = 0;
}

sub handle_TERM {
    my $self = shift;
    $self->{logger}->writeLogDebug("[prometheus] $$ Receiving order to stop...");
    $self->{stop} = 1;
}

sub class_handle_TERM {
    foreach (keys %{$handlers{TERM}}) {
        &{$handlers{TERM}->{$_}}();
    }
}

sub class_handle_HUP {
    foreach (keys %{$handlers{HUP}}) {
        &{$handlers{HUP}->{$_}}();
    }
}

sub load_hostgroups {
    my ($self, %options) = @_;

    my $conf_hostgroups = {};
    my $sth = $self->{db_centreon}->query({ query => 'SELECT hg_id, hg_name, hg_alias FROM `hostgroup`' });
    my $rows = [];
    while (my $row = (
            shift(@$rows) ||
            shift(@{$rows = $sth->fetchall_arrayref(undef,10_000) || []}) )
        ) {
        $conf_hostgroups->{ $row->[0] } = [ $row->[1], $row->[2] ];
    }

    $self->{hostgroups_excluded} = {};
    $self->{hostgroups_included} = {};
    $self->{hostgroups} = {};

    $sth = $self->{db_centstorage}->query({ query => 'SELECT tag_id, id, type, name FROM `tags`' });
    $rows = [];
    while (my $row = (
            shift(@$rows) ||
            shift(@{$rows = $sth->fetchall_arrayref(undef,10_000) || []}) ) 
        ) {
        next if ($row->[2] != 1);

        if ($self->{exclude_hosts_from_hg_matching} ne '' && $row->[3] =~ /$self->{exclude_hosts_from_hg_matching}/) {
            $self->{hostgroups_excluded}->{ $row->[0] } = 1;
            next;
        }

        # tag_id => [ hg_name, hg_alias ]
        $self->{hostgroups}->{ $row->[0] } = [
            $row->[3],
            defined($conf_hostgroups->{ $row->[1] }) ? $conf_hostgroups->{ $row->[1] }->[1] : ''
        ];

        if ($self->{filter_hosts_from_hg_matching} ne '' && $row->[3] =~ /$self->{filter_hosts_from_hg_matching}/) {
            $self->{hostgroups_included}->{ $row->[0] } = 1;
        }
        if ($self->{display_hg_matching} ne '' && $row->[3] !~ /$self->{display_hg_matching}/) {
            delete $self->{hostgroups}->{ $row->[0] };
        }
    }

    $self->{linked_host_hostgroups} = {};
    $self->{excluded_hosts} = {};
    $self->{included_hosts} = {};

    $sth = $self->{db_centstorage}->query({ query => 'SELECT tag_id, resource_id FROM `resources_tags`' });
    $rows = [];
    while (my $row = (
            shift(@$rows) ||
            shift(@{$rows = $sth->fetchall_arrayref(undef,10_000) || []}) ) 
        ) {
        if (defined($self->{hostgroups_excluded}->{ $row->[0] })) {
            $self->{excluded_hosts}->{ $row->[1] } = 1;
        }
        if ($self->{filter_hosts_from_hg_matching} ne '' && defined($self->{hostgroups_included}->{ $row->[0] })) {
            $self->{included_hosts}->{ $row->[1] } = 1;
        }
        next if (!defined($self->{hostgroups}->{ $row->[0] }));

        if (!defined($self->{linked_host_hostgroups}->{ $row->[1] })) {
            $self->{linked_host_hostgroups}->{ $row->[1] } = [];
        }

        push @{$self->{linked_host_hostgroups}->{ $row->[1] }}, $row->[0];
    }
}

sub load_metrics {
    my ($self, %options) = @_;

    return if ($connector->{add_metrics} == 0);

    $self->{metrics} = {};
    my $sth = $self->{db_centstorage}->query({ query => 'SELECT index_data.host_id, index_data.service_id, metrics.metric_name, metrics.current_value FROM index_data, metrics WHERE index_data.id = metrics.index_id' });
    my $rows = [];
    while (my $row = (
            shift(@$rows) ||
            shift(@{$rows = $sth->fetchall_arrayref(undef,10_000) || []}) ) 
        ) {
        # host_id . service_id => [ metric_name, metric_value ]
        if (!defined($self->{metrics}->{ $row->[0] . $row->[1] })) {
           $self->{metrics}->{ $row->[0] . $row->[1] } = []; 
        }

        push @{$self->{metrics}->{ $row->[0] . $row->[1] }}, [ $row->[2], $row->[3] ];
    }
}

sub load_hosts_services {
    my ($self, %options) = @_;

    $self->{hosts} = {};
    $self->{services} = {};
    $self->{linked_host_services} = {};

    # 0 - resource_id
    # 1 - id
    # 2 - parent_id
    # 3 - type (0 = service, 1 = host)
    # 4 - enabled
    # 5 - status
    # 6 - status_confirmed (0 = SOFT, 1 = HARD)
    # 7 - name
    # 8 - alias
    # 9 - acknowledged

    my $sth = $self->{db_centstorage}->query({ query => 'SELECT resource_id, id, parent_id, type, enabled, status, status_confirmed, name, alias, acknowledged FROM resources' });
    my $rows = [];
    while (my $row = (
            shift(@$rows) ||
            shift(@{$rows = $sth->fetchall_arrayref(undef,10_000) || []}) ) 
        ) {
        # ignore disabled resource
        next if ($row->[4] == 0);

        if ($row->[3] == 1) {
            $self->{hosts}->{ $row->[1] } = [
                $row->[0],
                $row->[7],
                $row->[8],
                $row->[5],
                $row->[6],
                $row->[9]
            ];
        } elsif ($row->[3] == 0) {
            $self->{services}->{ $row->[1] } = [
                $row->[0],
                $row->[7],
                $row->[8],
                $row->[5],
                $row->[6],
                $row->[9]
            ];
            if (!defined($self->{linked_host_services}->{ $row->[2] })) {
                $self->{linked_host_services}->{ $row->[2] } = [];
            }
            push @{$self->{linked_host_services}->{ $row->[2] }}, $row->[1];
        }
    }
}

sub check_host_from_hostgroups_rules {
    my ($self, %options) = @_;

    if (defined($self->{excluded_hosts}->{ $options{resource_id} })) {
        return 1;
    }
    if ($self->{filter_hosts_from_hg_matching} ne '' && !defined($self->{included_hosts}->{ $options{resource_id} })) {
        return 1;
    }

    return 0;
}

sub init_exporter {
    my ($self, %options) = @_;

    $self->{exporter_host_state_txt} = "$self->{host_state_type_metadata}
$self->{host_state_help_metadata}
";
    $self->{exporter_host_status_txt} = "$self->{host_status_type_metadata}
$self->{host_status_help_metadata}
";
    $self->{exporter_service_state_txt} = "$self->{service_state_type_metadata}
$self->{service_state_help_metadata}
";
    $self->{exporter_service_ack_txt} = "$self->{service_ack_type_metadata}
$self->{service_ack_help_metadata}
";
    $self->{exporter_service_status_txt} = "$self->{service_status_type_metadata}
$self->{service_status_help_metadata}
";
    $self->{exporter_metrics_txt} = '';
}

sub add_exporter_host_state {
    my ($self, %options) = @_;

    return if ($self->{add_state} == 0);

    my $value = $self->{host_state_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    $self->{exporter_host_state_txt} .= "$value\n";
}

sub add_exporter_host_status {
    my ($self, %options) = @_;

    return if ($self->{add_status} == 0);

    my $value = $self->{host_status_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    $self->{exporter_host_status_txt} .= "$value\n";
}

sub add_exporter_service_state {
    my ($self, %options) = @_;

    return if ($self->{add_state} == 0);

    my $value = $self->{service_state_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    $self->{exporter_service_state_txt} .= "$value\n";
}

sub add_exporter_service_status {
    my ($self, %options) = @_;

    return if ($self->{add_status} == 0);

    my $value = $self->{service_status_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    $self->{exporter_service_status_txt} .= "$value\n";
}

sub add_exporter_service_ack {
    my ($self, %options) = @_;

    return if ($self->{add_acknowledged} == 0);

    my $value = $self->{service_ack_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    $self->{exporter_service_ack_txt} .= "$value\n";
}

sub add_exporter_metric {
    my ($self, %options) = @_;

    my $value = $self->{metric_template};
    $value =~ s/%\((.*?)\)/$options{src}->{$1}/g;

    if ($self->{add_metrics_metadata} == 1) {
        $self->{exporter_metrics_txt} .= "# UNIT " . $options{src}->{metric_name} . " " . $options{src}->{metric_unit} . "\n";
    }

    $self->{exporter_metrics_txt} .= "$value\n";
}

sub map_host_attributes {
    my ($self, %options) = @_;

    my $element = {
        host_name => $options{host}->[1],
        host_alias => $options{host}->[2],
        host_status => $options{host}->[3],
        host_state => $options{host}->[4],
        host_acknowledged => $options{host}->[5],
        hostgroup_names => '',
        hostgroup_aliases => ''
    };

    my $append = '';
    foreach (@{$self->{linked_host_hostgroups}->{ $options{host}->[0] }}) {
        $element->{hostgroup_names} .= $append . $self->{hostgroups}->{$_}->[0];
        $element->{hostgroup_aliases} .= $append . $self->{hostgroups}->{$_}->[1];
        $append = ',';
    }

    return $element;
}

sub map_service_attributes {
    my ($self) = shift;

    $_[0]->{element}->{service_description} = $_[0]->{service}->[1];
    $_[0]->{element}->{service_status} = $_[0]->{service}->[3];
    $_[0]->{element}->{service_state} = $_[0]->{service}->[4];
    $_[0]->{element}->{service_acknowledged} = $_[0]->{service}->[5];
}

sub map_metric_attributes {
    my ($self) = shift;

    my @metric = split(/#/, $_[0]->{metric}->[0]);

    $_[0]->{element}->{metric_dimensions} = '';
    $_[0]->{element}->{metric_name} = $metric[0];
    if (defined($metric[1]) && $metric[1] ne '') {
        $_[0]->{element}->{metric_dimensions} = $metric[0];
        $_[0]->{element}->{metric_name} = $metric[1];
    }

    $_[0]->{element}->{metric_name} =~ s/\.([^.]+)$//;
    $_[0]->{element}->{metric_unit} = 'gauge';
    if ($1) {
        $_[0]->{element}->{metric_unit} = $1;
    }
    if ($_[0]->{element}->{metric_unit} eq 'count') {
        $_[0]->{element}->{metric_unit} = 'gauge';
    }

    $_[0]->{element}->{metric_name} =~ s/\./:/g;
    $_[0]->{element}->{metric_name} =~ s/[^a-zA-Z0-9_:]/_/g;
    $_[0]->{element}->{metric_value} = $_[0]->{metric}->[1];
}

sub export_to_file {
    my ($self, %options) = @_;

    return if ($self->{export_file} == 0);

    my $fh;
    if (!open($fh, '>', $self->{tmp_file})) {
        die "cannot open tmp file '" . $self->{tmp_file} . "': $!";
    }
    if ($self->{add_state} == 1) {
        print $fh $self->{exporter_host_state_txt} . "\n";
        print $fh $self->{exporter_service_state_txt} . "\n";
    }
    if ($self->{add_status} == 1) {
        print $fh $self->{exporter_host_status_txt} . "\n";
        print $fh $self->{exporter_service_status_txt} . "\n";
    }
    if ($self->{add_acknowledged} == 1) {
        print $fh $self->{exporter_service_ack_txt} . "\n";
    }
    if ($self->{add_metrics} == 1) {
        print $fh $self->{exporter_metrics_txt} . "\n";
    }
    close $fh;

    if (!rename($self->{tmp_file}, $self->{file})) {
        die "cannot rename tmp file '" . $self->{tmp_file} . "' to '" . $self->{file} . "': $!";
    }
}

sub export_to_gateway {
    my ($self, %options) = @_;

    return if ($self->{export_gateway} == 0);

    my %httpauth = ();
    if ($self->{prometheus_gateway_user} ne '') {
        $httpauth{credentials} = 1;
        $httpauth{basic} = 1;
        $httpauth{username} = $self->{prometheus_gateway_user};
        $httpauth{password} = $self->{prometheus_gateway_password};
    }
    my $curl_opts = [];
    if ($self->{prometheus_gateway_insecure} == 1) {
        $curl_opts = ['CURLOPT_SSL_VERIFYPEER => 0', 'CURLOPT_SSL_VERIFYHOST => 0'];
    }

    my ($status, $response);
    if ($self->{prometheus_gateway_wipe_last_time} < 0 || $self->{prometheus_gateway_wipe_last_time} < (time() - $self->{prometheus_gateway_wipe_interval})) {
        ($status, $response) = $self->{http}->request(
            method => 'DELETE',
            hostname => '',
            full_url => $self->{prometheus_gateway_address} . ':' . $self->{prometheus_gateway_port} . '/metrics/job/' . $self->{prometheus_gateway_job} . '/instance/' . $self->{prometheus_gateway_instance},
            %httpauth,
            curl_opt => $curl_opts,
            warning_status => '',
            unknown_status => '',
            critical_status => ''
        );
        if ($self->{http}->get_code() < 200 || $self->{http}->get_code() >= 300) {
            die "delete metrics to gateway error [code: '" . $self->{http}->get_code() . "'] [message: '" . $self->{http}->get_message() . "']";
        }
        $self->{prometheus_gateway_wipe_last_time} = time();
    }

    my $data = '';
    if ($self->{add_state} == 1) {
        $data .= $self->{exporter_host_state_txt} . "\n";
        $data .= $self->{exporter_service_state_txt} . "\n";
    }
    if ($self->{add_status} == 1) {
        $data .= $self->{exporter_host_status_txt} . "\n";
        $data .= $self->{exporter_service_status_txt} . "\n";
    }
    if ($self->{add_acknowledged} == 1) {
        $data .= $self->{exporter_service_ack_txt} . "\n";
    }
    if ($self->{add_metrics} == 1) {
        $data .= $self->{exporter_metrics_txt} . "\n";
    }

    ($status, $response) = $self->{http}->request(
        method => 'POST',
        hostname => '',
        full_url => $self->{prometheus_gateway_address} . ':' . $self->{prometheus_gateway_port} . '/metrics/job/' . $self->{prometheus_gateway_job} . '/instance/' . $self->{prometheus_gateway_instance},
        %httpauth, 
        header => [
            'Content-Type: application/openmetrics-text',
        ],
        query_form_post => $data,
        curl_opt => $curl_opts,
        warning_status => '',
        unknown_status => '',
        critical_status => ''
    );
    if ($self->{http}->get_code() < 200 || $self->{http}->get_code() >= 300) {
        die "post metrics to gateway error [code: '" . $self->{http}->get_code() . "'] [message: '" . $self->{http}->get_message() . "']";
    }
}

sub prometheus_exporter_update {
    my ($self, %options) = @_;

    $self->load_hostgroups();
    $self->load_metrics();
    $self->load_hosts_services();

    $self->init_exporter();

    foreach my $host_id (keys %{$self->{hosts}}) {
        next if ($self->check_host_from_hostgroups_rules(resource_id => $self->{hosts}->{$host_id}->[0]) == 1);

        my $element = $self->map_host_attributes(host => $self->{hosts}->{$host_id});

        $self->add_exporter_host_state(src => $element);
        $self->add_exporter_host_status(src => $element);

        foreach my $service_id (@{$self->{linked_host_services}->{$host_id}}) {
            $self->map_service_attributes({ element => $element, service => $self->{services}->{$service_id} });

            $self->add_exporter_service_state(src => $element);
            $self->add_exporter_service_status(src => $element);
            $self->add_exporter_service_ack(src => $element);

            next if (!defined($self->{metrics}->{$host_id . $service_id}));

            foreach my $metric (@{$self->{metrics}->{$host_id . $service_id}}) {
                $self->map_metric_attributes({ element => $element, metric => $metric });
                $self->add_exporter_metric(src => $element);
            }
        }
    }

    $self->export_to_file();
    $self->export_to_gateway();
}

sub action_centreonprometheusupdate {
    my ($self, %options) = @_;

    try {
        $self->{logger}->writeLogDebug('[prometheus] action update starting');
        $options{token} = $self->generate_token() if (!defined($options{token}));

        $self->send_log(code => GORGONE_ACTION_BEGIN, token => $options{token}, data => { message => 'action update starting' });

        $self->prometheus_exporter_update();

        $self->send_log(
            code => GORGONE_ACTION_FINISH_OK,
            token => $options{token},
            data => {
                message => 'action update finished'
            }
        );
        $self->{logger}->writeLogDebug('[prometheus] action update finished');
    } catch {
        my $error_message = $_;

        $self->send_log(
            code => GORGONE_ACTION_FINISH_KO,
            token => $options{token},
            data => {
                message => $error_message
            }
        );
        $self->{logger}->writeLogDebug('[prometheus] action update ' . $error_message);
    };

    return 0;
}

sub periodic_exec {
    if ($connector->{stop} == 1) {
        $connector->{logger}->writeLogInfo("[prometheus] $$ has quit");
        exit(0);
    }

    if ($connector->{update_last_time} < 0 || $connector->{update_last_time} < (time() - $connector->{update_interval})) {
        $connector->action_centreonprometheusupdate();
        $connector->{update_last_time} = time();
    }
}

sub run {
    my ($self, %options) = @_;

    $self->{internal_socket} = gorgone::standard::library::connect_com(
        context => $self->{zmq_context},
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgone-prometheus',
        logger => $self->{logger},
        type => $self->get_core_config(name => 'internal_com_type'),
        path => $self->get_core_config(name => 'internal_com_path')
    );
    $self->send_internal_action({
        action => 'CENTREONPROMETHEUSREADY',
        data => {}
    });

    if (defined($self->{config_db_centreon})) {
        $self->{db_centreon} = gorgone::class::db->new(
            dsn => $self->{config_db_centreon}->{dsn},
            user => $self->{config_db_centreon}->{username},
            password => $self->{config_db_centreon}->{password},
            force => 2,
            die => 1,
            logger => $self->{logger}
        );
    }

    if (defined($self->{config_db_centstorage})) {
        $self->{db_centstorage} = gorgone::class::db->new(
            dsn => $self->{config_db_centstorage}->{dsn},
            user => $self->{config_db_centstorage}->{username},
            password => $self->{config_db_centstorage}->{password},
            force => 2,
            die => 1,
            logger => $self->{logger}
        );
    }

    $self->{http} = gorgone::class::http::http->new(logger => $self->{logger});

    my $watcher_timer = $self->{loop}->timer(5, 5, \&periodic_exec);
    my $watcher_io = $self->{loop}->io($connector->{internal_socket}->get_fd(), EV::READ, sub { $connector->event() } );
    $self->{loop}->run();
}

1;
