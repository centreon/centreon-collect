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

package gorgone::modules::centreon::legacycmd::class;

use base qw(gorgone::class::module);

use strict;
use warnings;
use gorgone::standard::library;
use gorgone::standard::constants qw(:all);
use gorgone::standard::misc;
use gorgone::class::sqlquery;
use gorgone::class::tpapi::clapi;
use File::Copy;
use File::Path qw(make_path remove_tree);
use File::Find;
use File::Basename;
use MIME::Base64;
use EV;

my %handlers = (TERM => {}, HUP => {});
my ($connector);

sub new {
    my ($class, %options) = @_;
    $connector = $class->SUPER::new(%options);
    bless $connector, $class;

    $connector->{tpapi_clapi_name} = defined($options{config}->{tpapi_clapi}) && $options{config}->{tpapi_clapi} ne ''
        ? $options{config}->{tpapi_clapi}
        : 'clapi';
    if (!defined($connector->{config}->{cmd_file}) || $connector->{config}->{cmd_file} eq '') {
        $connector->{config}->{cmd_file} = '/var/lib/centreon/centcore.cmd';
    }
    if (!defined($connector->{config}->{cmd_dir}) || $connector->{config}->{cmd_dir} eq '') {
        $connector->{config}->{cmd_dir} = '/var/lib/centreon/centcore/';
    }
    $connector->{config}->{bulk_external_cmd} =
        defined($connector->{config}->{bulk_external_cmd}) && $connector->{config}->{bulk_external_cmd} =~ /(\d+)/ ? $1 : 50;
    $connector->{config}->{bulk_external_cmd_sequential} =
        defined($connector->{config}->{bulk_external_cmd_sequential}) && $connector->{config}->{bulk_external_cmd_sequential} =~ /^False|0$/i ? 0 : 1;
    $connector->{config}->{dirty_mode} = defined($connector->{config}->{dirty_mode}) ? $connector->{config}->{dirty_mode} : 1;
    $connector->{gorgone_illegal_characters} = '`';
    $connector->{cache_refresh_interval} = 60;
    $connector->{cache_refresh_last} = -1;
    $connector->{bulk_commands} = {};

    # web -> gorgone content transport (no shared /var/cache/centreon volume): the
    # web base64-encodes file content in the request, gorgone stages it here (own
    # local directory, scoped by request token) and uses it as REMOTECOPY source.
    # Never the same path as cache_dir: on a classic RPM install cache_dir IS the
    # web's own cache, writing there would race/overwrite what apache just generated.
    $connector->{config}->{payload_dir} = defined($connector->{config}->{payload_dir}) && $connector->{config}->{payload_dir} ne ''
        ? $connector->{config}->{payload_dir} : '/var/lib/centreon-gorgone/payload';
    $connector->{config}->{payload_max_age} = defined($connector->{config}->{payload_max_age}) && $connector->{config}->{payload_max_age} =~ /^\d+$/
        ? $connector->{config}->{payload_max_age} : 300;
    $connector->{payload_purge_interval} = 60;
    $connector->{payload_purge_last} = -1;

    # CENTREONCOMMAND batches pending an aggregated final result - see start_batch().
    $connector->{pending_batches} = {};
    $connector->{config}->{legacycmd_result_timeout} = defined($connector->{config}->{legacycmd_result_timeout}) && $connector->{config}->{legacycmd_result_timeout} =~ /^\d+$/
        ? $connector->{config}->{legacycmd_result_timeout} : 40;

    # nagios_server.id whose row means "gorgone and engine on the same host"
    # (classic VM/RPM install: localhost=1, poller_type != 'docker'). nodes'
    # DB sync (action_centreonnodessync) excludes that row from register_nodes
    # and treats it as the core instead, so REMOTECOPY/COMMAND can never reach
    # this id through the normal proxy path: execute_cmd must special case it.
    # Stays undef on docker/k8s TYPE=central, where the Central poller row is
    # registered like any other pullwss node - either localhost=0 (EP30
    # workaround row) or localhost=1 with poller_type='docker' (target model).
    $connector->{local_poller_id} = undef;

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
    $self->{logger}->writeLogDebug("[legacycmd] $$ Receiving order to stop...");
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

sub cache_refresh {
    my ($self, %options) = @_;

    return if ((time() - $self->{cache_refresh_interval}) < $self->{cache_refresh_last});

    # get pollers config
    $self->{pollers} = undef;
    my ($status, $datas) = $self->{class_object_centreon}->custom_execute(
        request => 'SELECT nagios_server_id, uid, command_file, cfg_dir, centreonbroker_cfg_path, snmp_trapd_path_conf, ' .
            'engine_start_command, engine_stop_command, engine_restart_command, engine_reload_command, ' .
            'broker_reload_command, init_script_centreontrapd, nagios_server.localhost, nagios_server.poller_type ' .
            'FROM cfg_nagios, nagios_server ' .
            "WHERE nagios_server.id = cfg_nagios.nagios_server_id AND cfg_nagios.nagios_activate = '1'",
        mode => 1,
        keys => 'nagios_server_id'
    );
    if ($status == -1 || !defined($datas)) {
        $self->{logger}->writeLogError('[legacycmd] Cannot get configuration for pollers');
        return ;
    }
    # Set only on success, so a startup DB failure retries next tick, not in cache_refresh_interval seconds.
    $self->{cache_refresh_last} = time();

    $self->{pollers} = $datas;
    $self->{local_poller_id} = undef;
    # both uid and id are used as key, but they contain the same pointer to the same data as value. This allow to reach a poller by both it's id and uid.
    for my $key (keys(%{$self->{pollers}})){
        my $uid = $self->{pollers}->{$key}->{uid};
        $self->{pollers}->{$uid} = $self->{pollers}->{$key};
        # localhost=1 means "collocated with gorgone" only outside a
        # docker/k8s split (poller_type='docker'): there, localhost=1 on the
        # Central row just means "this is the Central poller", reached like
        # any other node (pullwss) - see the matching fix in
        # gorgone::modules::centreon::nodes::class.pm action_centreonnodessync.
        $self->{local_poller_id} = $key
            if (defined($self->{pollers}->{$key}->{localhost}) && $self->{pollers}->{$key}->{localhost} == 1
                && (!defined($self->{pollers}->{$key}->{poller_type}) || $self->{pollers}->{$key}->{poller_type} ne 'docker'));
    }

    # check illegal characters
    ($status, $datas) = $self->{class_object_centreon}->custom_execute(
        request => "SELECT `value` FROM options WHERE `key` = 'gorgone_illegal_characters'",
        mode => 2
    );
    if ($status == -1) {
        $self->{logger}->writeLogError('[legacycmd] Cannot get illegal characters');
        return ;
    }

    if (defined($datas->[0]->[0])) {
        $self->{gorgone_illegal_characters} = $datas->[0]->[0];
    }
}

sub check_pollers_config {
    my ($self, %options) = @_;

    return defined($self->{pollers}) ? 1 : 0;
}

sub send_external_commands {
    my ($self, %options) = @_;
    my $token = $options{token};
    $token = $self->generate_token() if (!defined($token));

    my $targets = [];
    $targets = [$options{target}] if (defined($options{target}));
    if (scalar(@$targets) <= 0) {
        $targets = [keys %{$self->{bulk_commands}}];
    }

    foreach my $target (@$targets) {
        next if (!defined($self->{bulk_commands}->{$target}) || scalar(@{$self->{bulk_commands}->{$target}}) <= 0);
        # ENGINECOMMAND to the central's own poller can't route through proxy
        # (register_nodes excludes localhost=1): target => undef runs it locally.
        my $dispatch_target = (defined($self->{local_poller_id}) && $target eq $self->{local_poller_id}) ? undef : $target;
        $self->send_internal_action({
            action => 'ENGINECOMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command_file => $self->{pollers}->{$target}->{command_file},
                    commands => [
                        join("\n", @{$self->{bulk_commands}->{$target}})
                    ]
                }
            }
        });

        $self->{logger}->writeLogDebug("[legacycmd] send external commands for '$target'");
        $self->{bulk_commands}->{$target} = [];
    }
}

sub add_external_command {
    my ($self, %options) = @_;

    $options{param} =~ s/[\Q$self->{gorgone_illegal_characters}\E]//g
        if (defined($self->{gorgone_illegal_characters}) && $self->{gorgone_illegal_characters} ne '');
    if ($options{action} == 1) {
        my $dispatch_target = (defined($self->{local_poller_id}) && defined($options{target}) && $options{target} eq $self->{local_poller_id})
            ? undef : $options{target};
        $self->send_internal_action({
            action => 'ENGINECOMMAND',
            target => $dispatch_target,
            token => $options{token},
            data => {
                logging => $options{logging},
                content => {
                    command_file => $self->{pollers}->{ $options{target} }->{command_file},
                    commands => [
                        $options{param}
                    ]
                }
            }
        });
    } else {
        $self->{bulk_commands}->{ $options{target} } = [] if (!defined($self->{bulk_commands}->{ $options{target} }));
        push @{$self->{bulk_commands}->{ $options{target} }}, $options{param};
        if (scalar(@{$self->{bulk_commands}->{ $options{target} }}) > $self->{config}->{bulk_external_cmd}) {
            $self->send_external_commands(%options);
        }
    }
}

# Commands whose dispatched sub-action(s) log exactly one terminal entry each -
# safe to wait on and aggregate (EXTERNALCMD/SENDEXPORTFILE/STARTWORKER don't).
my %BATCH_SAFE_COMMANDS = map { $_ => 1 } (
    'SENDCFGFILE', 'SYNCTRAP', 'ENGINERESTART', 'RESTART', 'ENGINERELOAD', 'RELOAD',
    'START', 'STOP', 'RELOADBROKER', 'RESTARTCENTREONTRAPD', 'RELOADCENTREONTRAPD', 'VMWARERESTART'
);

# Counts every sub-action a tracked batch dispatches; no-op otherwise.
sub send_internal_action {
    my ($self, $options) = (shift, shift);

    if (defined($options->{token}) && defined($options->{action})
        && $options->{action} =~ /^(?:REMOTECOPY|ACTIONENGINE|COMMAND)$/
        && defined($self->{pending_batches}->{ $options->{token} })) {
        my $batch = $self->{pending_batches}->{ $options->{token} };
        $batch->{expected}++;
        push @{$batch->{results}}, { target => $options->{target}, command => $options->{action}, status => 'pending' };
        $batch->{targets}->{ $options->{target} } = 1 if (defined($options->{target}) && $options->{target} ne '');
    }

    return $self->SUPER::send_internal_action($options, @_);
}

sub start_batch {
    my ($self, %options) = @_;

    $self->{pending_batches}->{ $options{token} } = {
        expected => 0,
        results => [],
        targets => {},
        deadline => time() + $self->{config}->{legacycmd_result_timeout}
    };
}

# Always logs exactly one final entry for the token, tracked batch or not -
# the web only ever waits on this marker, never on gorgone's initial accept.
sub finalize_batch {
    my ($self, %options) = @_;

    my $batch = delete $self->{pending_batches}->{ $options{token} };
    my $results = defined($batch) ? $batch->{results} : [];
    if ($options{code} == GORGONE_ACTION_FINISH_OK) {
        $_->{status} = 'ok' foreach (@$results);
    }

    $self->send_log(
        code => $options{code},
        token => $options{token},
        data => { message => $options{message}, legacycmd_final => 1, tracked => defined($batch) ? 1 : 0, results => $results }
    );
}

# Reports an immediate success when the batch dispatched nothing async.
sub finalize_batch_if_idle {
    my ($self, %options) = @_;

    my $batch = $self->{pending_batches}->{ $options{token} };
    return if (!defined($batch) || $batch->{expected} > 0);

    $self->finalize_batch(token => $options{token}, code => GORGONE_ACTION_FINISH_OK, message => 'centreoncommand finished successfully');
}

# Periodic sweep (every 5s): nudges each poller's SETLOGS via GETLOG, then
# polls this central's own history for the batch token - see action_ack.
sub check_pending_batches {
    my ($self) = @_;

    my $ctime = time();
    foreach my $token (keys %{$self->{pending_batches}}) {
        my $batch = $self->{pending_batches}->{$token};

        if ($ctime > $batch->{deadline}) {
            $self->finalize_batch(token => $token, code => GORGONE_ACTION_FINISH_KO, message => 'timeout waiting for sub-actions to finish');
            next;
        }

        foreach my $target (keys %{$batch->{targets}}) {
            $self->send_internal_action({ action => 'GETLOG', target => $target, data => {} });
        }

        # No cursor: recounts every row for the token from scratch each time.
        $self->send_internal_action({
            action => 'GETLOG',
            token => $token . '-poll',
            data => { token => $token }
        });
    }
}

# Reply to the local GETLOG poll above (see event() for why it reaches here).
sub action_ack {
    my ($self, %options) = @_;

    return if (!defined($options{token}) || $options{token} !~ /^(.*)-poll$/);
    my $token = $1;
    my $batch = $self->{pending_batches}->{$token};
    return if (!defined($batch));

    my $rows = (defined($options{data}->{data}->{result})) ? $options{data}->{data}->{result} : [];
    my ($ok_count, $ko_message) = (0, undef);
    foreach my $row (@$rows) {
        next if ($row->{code} != GORGONE_ACTION_FINISH_OK && $row->{code} != GORGONE_ACTION_FINISH_KO);

        my ($rv, $data) = $self->json_decode(argument => $row->{data});
        next if ($rv || defined($data->{legacycmd_final}));

        if ($row->{code} == GORGONE_ACTION_FINISH_KO) {
            $ko_message = defined($data->{message}) ? $data->{message} : 'sub-action failed';
            last;
        }
        $ok_count++;
    }

    if (defined($ko_message)) {
        $self->finalize_batch(token => $token, code => GORGONE_ACTION_FINISH_KO, message => $ko_message);
    } elsif ($ok_count >= $batch->{expected}) {
        $self->finalize_batch(token => $token, code => GORGONE_ACTION_FINISH_OK, message => 'centreoncommand finished successfully');
    }
}

# A local GETLOG's reply has two bracket groups ("[ACK] [token] {...}"), not
# three like a routed message - module.pm's event() only parses the latter.
sub event {
    my ($self, %options) = @_;

    my $socket = defined($options{socket}) ? $options{socket} : $self->{internal_socket};
    while ($socket->has_pollin()) {
        my ($message) = $self->read_message();
        next if (!defined($message));

        $self->{logger}->writeLogDebug("[$self->{module_id}]$self->{container} Event: $message");
        if ($message =~ /^\[([^\]]*)\]\s+\[([^\]]*)\]\s+\[.*?\]\s+(.*)$/m || $message =~ /^\[([^\]]*)\]\s+\[([^\]]*)\]\s+(.*)$/m) {
            my ($action, $token, $raw) = ($1, $2, $3);
            if ((my $method = $self->can('action_' . lc($action)))) {
                # ACK answers every dispatch this module makes, not just our GETLOG polls:
                # a decode failure must stay silent, not KO whatever token that was.
                my ($rv, $data) = $action eq 'ACK'
                    ? $self->json_decode(argument => $raw)
                    : $self->json_decode(argument => $raw, token => $token);
                $method->($self, token => $token, data => $data) if (!$rv);
            }
        }
    }
}

sub execute_cmd {
    my ($self, %options) = @_;

    chomp $options{target};
    chomp $options{param} if (defined($options{param}));
    my $token = $options{token};
    $token = $self->generate_token() if (!defined($token));

    # COMMAND/ACTIONENGINE targeting the central's own poller (localhost=1,
    # classic RPM install) can't route through proxy (register_nodes excludes
    # it): target => undef hits core.pm's own self-shortcut instead, same
    # mechanism STARTWORKER already uses to run locally.
    my $dispatch_target = (defined($self->{local_poller_id}) && defined($options{target}) && $options{target} eq $self->{local_poller_id})
        ? undef : $options{target};

    my $msg = "[legacycmd] Handling command '" . $options{cmd} . "'";
    $msg .= ", Target: '" . $options{target} . "'" if (defined($options{target}));
    $msg .= ", Parameters: '" . $options{param} . "'" if (defined($options{param}));
    $self->{logger}->writeLogInfo($msg);

    if ($options{cmd} eq 'EXTERNALCMD') {
        $self->add_external_command(
            action => $options{action},
            param => $options{param},
            target => $options{target},
            token => $options{token},
            logging => $options{logging}
        );
        return 0;
    }

    $self->send_external_commands(target => $options{target})
        if (defined($options{target}) && $self->{config}->{bulk_external_cmd_sequential} == 1);

    if ($options{cmd} eq 'SENDCFGFILE') {
        my $cache_dir = (defined($connector->{config}->{cache_dir}) && $connector->{config}->{cache_dir} ne '') ?
            $connector->{config}->{cache_dir} : '/var/cache/centreon';
        my $files = $options{files};
        my $is_local = defined($self->{local_poller_id}) && $options{target} eq $self->{local_poller_id};

        # validate everything up front: reject the whole request, write nothing,
        # rather than leave a partial set of components applied.
        if (defined($files)) {
            if ($self->validate_payload_target(target => $options{target}) == -1) {
                return (-1, "SENDCFGFILE: invalid target '" . $options{target} . "'");
            }
            foreach my $component ('vmware', 'engine', 'broker') {
                next if (!defined($files->{$component}));
                my ($vstatus, $vmessage) = $self->validate_payload_files(files => $files->{$component}, relative => 0);
                return (-1, "SENDCFGFILE: $vmessage") if ($vstatus == -1);
            }
        }

        my %sources = (
            vmware => $cache_dir . '/config/vmware/' . $options{target} . '/centreon_vmware.json',
            engine => $cache_dir . '/config/engine/' . $options{target},
            broker => $cache_dir . '/config/broker/' . $options{target}
        );
        my %skip;
        foreach my $component ('vmware', 'engine', 'broker') {
            next if (!defined($files) || !defined($files->{$component}));
            if (scalar(@{$files->{$component}}) == 0) {
                $skip{$component} = 1;
                next;
            }
            my ($wstatus, $wresult) = $self->write_payload_files(
                token => $token,
                component => $component,
                files => $files->{$component}
            );
            return (-1, "SENDCFGFILE: $wresult") if ($wstatus == -1);
            # engine/broker REMOTECOPY source is a directory (whole cfg_dir);
            # vmware is a single regular file, so point source at the file
            # itself, not its containing directory (prepare_remote_copy picks
            # regular-file vs archive mode from -f/-d on source).
            $sources{$component} = $component eq 'vmware' ? $wresult . '/' . $files->{$component}->[0]->{name} : $wresult;
        }

        my %dispatch = (
            vmware => {
                source => $sources{vmware},
                destination => '/etc/centreon/centreon_vmware.json',
                owner => 'centreon-gorgone', # gorgoned run under centreon-gorgone user. On linux only root can change the ownership of a file.
                group => 'centreon'
            },
            engine => {
                source => $sources{engine},
                destination => $self->{pollers}->{$options{target}}->{cfg_dir} . '/',
                owner => 'centreon-engine',
                group => 'centreon-engine'
            },
            broker => {
                source => $sources{broker},
                destination => $self->{pollers}->{$options{target}}->{centreonbroker_cfg_path} . '/',
                owner => 'centreon-broker',
                group => 'centreon-broker'
            }
        );
        foreach my $component ('vmware', 'engine', 'broker') {
            next if ($skip{$component});

            if ($is_local) {
                my ($lstatus, $lmessage) = $self->local_copy_path(
                    source => $dispatch{$component}->{source},
                    destination => $dispatch{$component}->{destination},
                    owner => $dispatch{$component}->{owner},
                    group => $dispatch{$component}->{group}
                );
                return (-1, "SENDCFGFILE: $lmessage") if ($lstatus == -1);
                next;
            }

            $self->send_internal_action({
                action => 'REMOTECOPY',
                target => $options{target},
                token => $token,
                data => {
                    logging => $options{logging},
                    content => {
                        source => $dispatch{$component}->{source},
                        destination => $dispatch{$component}->{destination},
                        cache_dir => $cache_dir,
                        owner => $dispatch{$component}->{owner},
                        group => $dispatch{$component}->{group},
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'SENDCFGFILE'
                        }
                    }
                }
            });
        }
    } elsif ($options{cmd} eq 'SENDEXPORTFILE') {
        if (!defined($self->{clapi_password})) {
            return (-1, 'need centreon clapi password to execute SENDEXPORTFILE command');
        }

        my $cache_dir = (defined($connector->{config}->{cache_dir}) && $connector->{config}->{cache_dir} ne '') ?
            $connector->{config}->{cache_dir} : '/var/cache/centreon';
        my $remote_dir = (defined($connector->{config}->{remote_dir})) ?
            $connector->{config}->{remote_dir} : '/var/cache/centreon/config/remote-data/';
        my $files = $options{files};
        my $source = $cache_dir . '/config/export/' . $options{target};
        my $skip = 0;

        if (defined($files) && defined($files->{export})) {
            if ($self->validate_payload_target(target => $options{target}) == -1) {
                return (-1, "SENDEXPORTFILE: invalid target '" . $options{target} . "'");
            }
            # export carries sub-folders (media/...): relative paths allowed.
            my ($vstatus, $vmessage) = $self->validate_payload_files(files => $files->{export}, relative => 1);
            return (-1, "SENDEXPORTFILE: $vmessage") if ($vstatus == -1);

            if (scalar(@{$files->{export}}) == 0) {
                $skip = 1;
            } else {
                my ($wstatus, $wresult) = $self->write_payload_files(
                    token => $token,
                    component => 'export',
                    files => $files->{export}
                );
                return (-1, "SENDEXPORTFILE: $wresult") if ($wstatus == -1);
                $source = $wresult;
            }
        }

        # remote server
        if (!$skip) {
            if (defined($self->{local_poller_id}) && $options{target} eq $self->{local_poller_id}) {
                my ($lstatus, $lmessage) = $self->local_copy_path(
                    source => $source,
                    destination => $remote_dir,
                    owner => 'centreon',
                    group => 'centreon'
                );
                return (-1, "SENDEXPORTFILE: $lmessage") if ($lstatus == -1);
            } else {
                $self->send_internal_action({
                    action => 'REMOTECOPY',
                    target => $options{target},
                    token => $token,
                    data => {
                        logging => $options{logging},
                        content => {
                            source => $source,
                            destination => $remote_dir,
                            cache_dir => $cache_dir,
                            owner => 'centreon',
                            group => 'centreon',
                            metadata => {
                                centcore_cmd => 'SENDEXPORTFILE'
                            }
                        }
                    }
                });
            }
        }

        # Forward data use to be done by createRemoteTask as well as task_id in a gorgone command
        # Command name: AddImportTaskWithParent
        # Data: ['parent_id' => $task->getId()]
        $self->send_internal_action({
            action => 'ADDIMPORTTASKWITHPARENT',
            token => $options{token},
            target => $options{target},
            data => {
                logging => $options{logging},
                content => {
                    parent_id => $options{param},
                    cbd_reload => 'sudo ' . $self->{pollers}->{ $options{target} }->{broker_reload_command}
                }
            }
        });
    } elsif ($options{cmd} eq 'SYNCTRAP') {
        my $cache_dir = (defined($connector->{config}->{cache_dir}) && $connector->{config}->{cache_dir} ne '') ?
            $connector->{config}->{cache_dir} : '/var/cache/centreon';
        my $cache_dir_trap = (defined($connector->{config}->{cache_dir_trap}) && $connector->{config}->{cache_dir_trap} ne '') ?
            $connector->{config}->{cache_dir_trap} : '/etc/snmp/centreon_traps/';
        my $files = $options{files};
        my $source = $cache_dir_trap . '/' . $options{target} . '/centreontrapd.sdb';
        my $skip = 0;

        if (defined($files) && defined($files->{trap})) {
            if ($self->validate_payload_target(target => $options{target}) == -1) {
                return (-1, "SYNCTRAP: invalid target '" . $options{target} . "'");
            }
            my ($vstatus, $vmessage) = $self->validate_payload_files(files => $files->{trap}, relative => 0);
            return (-1, "SYNCTRAP: $vmessage") if ($vstatus == -1);

            if (scalar(@{$files->{trap}}) == 0) {
                $skip = 1;
            } else {
                my ($wstatus, $wresult) = $self->write_payload_files(
                    token => $token,
                    component => 'trap',
                    files => $files->{trap}
                );
                return (-1, "SYNCTRAP: $wresult") if ($wstatus == -1);
                $source = $wresult . '/' . $files->{trap}->[0]->{name};
            }
        }

        # centreontrapd
        if (!$skip) {
            if (defined($self->{local_poller_id}) && $options{target} eq $self->{local_poller_id}) {
                my ($lstatus, $lmessage) = $self->local_copy_path(
                    source => $source,
                    destination => $self->{pollers}->{$options{target}}->{snmp_trapd_path_conf} . '/',
                    owner => 'centreon',
                    group => 'centreon',
                    mode => '0664'
                );
                return (-1, "SYNCTRAP: $lmessage") if ($lstatus == -1);
            } else {
                $self->send_internal_action({
                    action => 'REMOTECOPY',
                    target => $options{target},
                    token => $token,
                    data => {
                        logging => $options{logging},
                        content => {
                            source => $source,
                            destination => $self->{pollers}->{$options{target}}->{snmp_trapd_path_conf} . '/',
                            cache_dir => $cache_dir,
                            owner => 'centreon',
                            group => 'centreon',
                            # With SYNCTRAP the destination file must have permissions 0664
                            mode => '0664',
                            metadata => {
                                centcore_proxy => 1,
                                centcore_cmd => 'SYNCTRAP'
                            }
                        }
                    }
                });
            }
        }
    } elsif ($options{cmd} eq 'ENGINERESTART') {
        # restart centreon-engine. 'command' is the fallback (sudo systemctl
        # restart) used only on nodes where the action module's engine_grpc is
        # not enabled; 'grpc' lets a gRPC-capable node skip the shell entirely
        # (gorgone::modules::core::action::class::action_actionengine).
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_restart_command', cmd => 'ENGINERESTART');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'ACTIONENGINE',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command => 'sudo ' . $cmd,
                    plugins => $self->{pollers}->{ $options{target} }->{cfg_dir} . '/plugins.json',
                    grpc => { signal => 'restart' },
                    metadata => {
                        centcore_proxy => 1,
                        centcore_cmd => 'ENGINERESTART'
                    }
                }
            }
        });
    } elsif ($options{cmd} eq 'RESTART') {
        # restart centreon-engine. Dispatched via ACTIONENGINE (not COMMAND)
        # so it goes through the same gRPC-capable path as ENGINERESTART
        # above - the two centcore commands do the exact same thing.
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_restart_command', cmd => 'RESTART');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'ACTIONENGINE',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command => 'sudo ' . $cmd,
                    grpc => { signal => 'restart' },
                    metadata => {
                        centcore_proxy => 1,
                        centcore_cmd => 'RESTART'
                    }
                }
            }
        });
    } elsif ($options{cmd} eq 'ENGINERELOAD') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_reload_command', cmd => 'ENGINERELOAD');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'ACTIONENGINE',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command => 'sudo ' . $cmd,
                    plugins => $self->{pollers}->{ $options{target} }->{cfg_dir} . '/plugins.json',
                    grpc => { signal => 'reload' },
                    metadata => {
                        centcore_proxy => 1,
                        centcore_cmd => 'ENGINERELOAD'
                    }
                }
            }
        });
    } elsif ($options{cmd} eq 'RELOAD') {
        # dispatched via ACTIONENGINE (not COMMAND): see RESTART above.
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_reload_command', cmd => 'RELOAD');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'ACTIONENGINE',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command => 'sudo ' . $cmd,
                    grpc => { signal => 'reload' },
                    metadata => {
                        centcore_proxy => 1,
                        centcore_cmd => 'RELOAD'
                    }
                }
            }
        });
    } elsif ($options{cmd} eq 'START') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_start_command', cmd => 'START');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'COMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => 'sudo ' . $cmd,
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'START'
                        }
                    }
                ]
            }
        });
    } elsif ($options{cmd} eq 'STOP') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'engine_stop_command', cmd => 'STOP');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'COMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => 'sudo ' . $cmd,
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'STOP'
                        }
                    }
                ]
            }
        });
    } elsif ($options{cmd} eq 'RELOADBROKER') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'broker_reload_command', cmd => 'RELOADBROKER');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'COMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => 'sudo ' . $cmd,
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'RELOADBROKER'
                        }
                    }
                ]
            }
        });
    } elsif ($options{cmd} eq 'RESTARTCENTREONTRAPD') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'init_script_centreontrapd', cmd => 'RESTARTCENTREONTRAPD');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'COMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => 'sudo service ' . $cmd . ' restart',
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'RESTARTCENTREONTRAPD'
                        }
                    }
                ]
            }
        });
    } elsif ($options{cmd} eq 'RELOADCENTREONTRAPD') {
        my ($cmd, $cmd_error) = $self->resolve_poller_command(target => $options{target}, field => 'init_script_centreontrapd', cmd => 'RELOADCENTREONTRAPD');
        return (-1, $cmd_error) if (!defined($cmd));
        $self->send_internal_action({
            action => 'COMMAND',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => 'sudo service ' . $cmd . ' reload',
                        metadata => {
                            centcore_proxy => 1,
                            centcore_cmd => 'RELOADCENTREONTRAPD'
                        }
                    }
                ]
            }
        });
    } elsif ($options{cmd} eq 'STARTWORKER') {
        if (!defined($self->{clapi_password})) {
            return (-1, 'need centreon clapi password to execute STARTWORKER command');
        }
        my $task_timeout;
        # need to send to clapi the timeout that gorgone is using from the action module
        foreach my $module (@{$self->{config_core}->{modules}}) {
            if ($module->{package} eq 'gorgone::modules::core::action::hooks') {
                $task_timeout = $module->{command_timeout};
                last
            }
        }
        $task_timeout ||= 60;

        my $centreon_dir = (defined($connector->{config}->{centreon_dir})) ?
            $connector->{config}->{centreon_dir} : '/usr/share/centreon';
        my $cmd = $centreon_dir . '/bin/centreon -u "' . $self->{clapi_user} . '" -p "' .
            $self->{clapi_password} . '" -w -o CentreonWorker -a processQueue --commandTimeout '.$task_timeout;
        $self->send_internal_action({
            action => 'COMMAND',
            target => undef,
            token => $token,
            data => {
                logging => $options{logging},
                content => [
                    {
                        command => $cmd,
                        timeout => $task_timeout,
                        metadata => {
                            centcore_cmd => 'STARTWORKER'
                        }
                    }
                ]
            }
        });
    }
    elsif ($options{cmd} eq 'VMWARERESTART') {

        $self->send_internal_action({
            action => 'ACTIONENGINE',
            target => $dispatch_target,
            token => $token,
            data => {
                logging => $options{logging},
                content => {
                    command => 'sudo systemctl restart centreon_vmware.service',
                }
            }
        });
    } else{
        $self->{logger}->writeLogWarning('[legacycmd] Cannot process message type ' . $options{cmd} . "throwing it away.");
    }

    return 0;
}

sub action_addimporttaskwithparent {
    my ($self, %options) = @_;

    if (!defined($options{data}->{content}->{parent_id})) {
        $self->send_log(
            code => GORGONE_ACTION_FINISH_KO,
            token => $options{token},
            logging => $options{data}->{logging},
            data => {
                message => "expected parent_id task ID",
            }
        );
        return -1;
    }

    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time());
    my $datetime = sprintf('%04d-%02d-%02d %02d:%02d:%02d', $year+1900, $mon+1, $mday, $hour, $min, $sec);

    my ($status, $datas) = $self->{class_object_centreon}->custom_execute(
        request => "INSERT INTO task (`type`, `status`, `parent_id`, `created_at`) VALUES ('import', 'pending', '" . $options{data}->{content}->{parent_id} . "', '" . $datetime . "')"
    );
    if ($status == -1) {
        $self->send_log(
            code => GORGONE_ACTION_FINISH_KO,
            token => $options{token},
            logging => $options{data}->{logging},
            data => {
                message => "Cannot add import task on Remote Server.",
            }
        );
        return -1;
    }

    my $task_timeout;
    foreach my $module (@{$self->{config_core}->{modules}}) {
        # need to send to clapi the timeout that gorgone is using from the action module
        if ($module->{package} eq 'gorgone::modules::core::action::hooks') {
            $task_timeout = $module->{command_timeout};
            last
        }
    }

    $task_timeout ||= 60;

    my $centreon_dir = $connector->{config}->{centreon_dir} // '/usr/share/centreon';
    my $cmd = $centreon_dir . '/bin/centreon -u "' . $self->{clapi_user} . '" -p "' .
        $self->{clapi_password} . '" -w -o CentreonWorker -a processQueue --commandTimeout '.$task_timeout;
    $self->send_internal_action({
        action => 'COMMAND',
        token => $options{token},
        data => {
            logging => $options{data}->{logging},
            content => [
                {
                    command => $cmd,
                    timeout => $task_timeout,
                }
            ],
            parameters => { no_fork => 1 }
        }
    });
    $self->send_internal_action({
        action => 'COMMAND',
        token => $options{token},
        data => {
            logging => $options{data}->{logging},
            content => [
                {
                    command => $options{data}->{content}->{cbd_reload}
                }
            ]
        }
    });

    $self->send_log(
        code => GORGONE_ACTION_FINISH_OK,
        token => $options{token},
        logging => $options{data}->{logging},
        data => {
            message => 'Task inserted on Remote Server',
        }
    );

    return 0;
}

sub move_cmd_file {
    my ($self, %options) = @_;

    my $operator = '+<';
    if ($self->{config}->{dirty_mode} == 1) {
        $operator = '<';
    }
    my $handle;
    if (-e $options{dst}) {
        if (!open($handle, $operator, $options{dst})) {
            $self->{logger}->writeLogError("[legacycmd] Cannot open file '" . $options{dst} . "': $!");
            return -1;
        }
        
        return (0, $handle);
    }

    return -1 if (!defined($options{src}));
    return -1 if (! -e $options{src});

    if (!File::Copy::move($options{src}, $options{dst})) {
        $self->{logger}->writeLogError("[legacycmd] Cannot move file '" . $options{src} . "': $!");
        return -1;
    }

    if (!open($handle, $operator, $options{dst})) {
        $self->{logger}->writeLogError("[legacycmd] Cannot open file '" . $options{dst} . "': $!");
        return -1;
    }

    return (0, $handle);
}

sub handle_file {
    my ($self, %options) = @_;
    require bytes;

    $self->{logger}->writeLogDebug("[legacycmd] Processing file '" . $options{file} . "'");
    my $handle = $options{handle};
    while (my $line = <$handle>) {
        if ($self->{stop} == 1) {
            close($handle);
            return -1;
        }

        if ($line =~ /^(.*?):([^:]*)(?::(.*)){0,1}/) {
            $self->execute_cmd(action => 0, cmd => $1, target => $2, param => $3, logging => 0);
            if ($self->{config}->{dirty_mode} != 1) {
                my $current_pos = tell($handle);
                seek($handle, $current_pos - bytes::length($line), 0);
                syswrite($handle, '-');
                # line is useless
                $line = <$handle>;
            }
        }
    }

    close($handle);
    unlink($options{file});
    return 0;
}

sub handle_centcore_cmd {
    my ($self, %options) = @_;

    my ($code, $handle) = $self->move_cmd_file(
        src => $self->{config}->{cmd_file},
        dst => $self->{config}->{cmd_file} . '_read',
    );
    return if ($code == -1);
    $self->handle_file(handle => $handle, file => $self->{config}->{cmd_file} . '_read');
}

sub handle_centcore_dir {
    my ($self, %options) = @_;
    
    my ($dh, @files);
    if (!opendir($dh, $self->{config}->{cmd_dir})) {
        $self->{logger}->writeLogError("[legacycmd] Cannot open directory '" . $self->{config}->{cmd_dir} . "': $!");
        return ;
    }
    @files = sort {
        (stat($self->{config}->{cmd_dir} . '/' . $a))[10] <=> (stat($self->{config}->{cmd_dir} . '/' . $b))[10]
    } (readdir($dh));
    closedir($dh);

    my ($code, $handle);
    foreach (@files) {
        next if ($_ =~ /^\./);
        my $file = $self->{config}->{cmd_dir} . '/' . $_;
        if ($file =~ /_read$/) {
            ($code, $handle) = $self->move_cmd_file(
                dst => $file,
            );
        } else {
            ($code, $handle) = $self->move_cmd_file(
                src => $file,
                dst => $file . '_read',
            );
            $file .= '_read';
        }
        return if ($code == -1);
        if ($self->handle_file(handle => $handle, file => $file) == -1) {
            return ;
        }
    }
}

sub handle_cmd_files {
    my ($self, %options) = @_;

    return if ($self->check_pollers_config() == 0);
    $self->handle_centcore_cmd();
    $self->handle_centcore_dir();
    $self->send_external_commands(logging => 0);
}

# --- web -> gorgone file content transport (no shared /var/cache/centreon volume) ---

# engine_*_command/broker_reload_command/init_script_centreontrapd can be
# empty/unset in cfg_nagios for a given poller (misconfiguration, or a row
# never meant to run that action). Building 'sudo ' . undef then shipping it
# produces a confusing "uninitialized value" warning here and a whitelist
# rejection downstream instead of a clear error at the source.
sub resolve_poller_command {
    my ($self, %options) = @_;

    my $cmd = $self->{pollers}->{ $options{target} }->{ $options{field} };
    if (!defined($cmd) || $cmd eq '') {
        return (undef, "$options{cmd}: no '$options{field}' configured for poller '$options{target}'");
    }

    return ($cmd, undef);
}

sub validate_payload_target {
    my ($self, %options) = @_;

    return 0 if (defined($options{target}) && $options{target} =~ /^\d+$/);
    return -1;
}

# component != 'export': flat filename, no path separator.
# component == 'export': relative path allowed (media subfolders), no leading
# '/', no '..' segment, no NUL.
sub validate_payload_name {
    my ($self, %options) = @_;

    my $name = $options{name};
    return -1 if (!defined($name) || $name eq '' || $name =~ /\x00/);

    if ($options{relative}) {
        return -1 if ($name =~ /^\//);
        foreach my $segment (split(/\//, $name)) {
            return -1 if ($segment eq '' || $segment eq '.' || $segment eq '..');
            return -1 if ($segment !~ /^[A-Za-z0-9._-]+$/);
        }
        return 0;
    }

    return 0 if ($name =~ /^[A-Za-z0-9._-]+$/);
    return -1;
}

sub validate_payload_files {
    my ($self, %options) = @_;

    foreach my $file (@{$options{files}}) {
        if (ref($file) ne 'HASH' || !defined($file->{name}) || !defined($file->{content})) {
            return (-1, 'invalid file entry (expected {name, content})');
        }
        if ($self->validate_payload_name(name => $file->{name}, relative => $options{relative}) == -1) {
            return (-1, "invalid file name '" . $file->{name} . "'");
        }
    }

    return 0;
}

# writes base64 content into gorgone's own staging directory (never the shared
# cache_dir: on a classic install that path is the web's cache, writing there
# races with the next config generation). Returns the staging directory, used
# unchanged as REMOTECOPY source afterwards. Stale staging directories are
# removed later by purge_old_payloads().
sub write_payload_files {
    my ($self, %options) = @_;

    my $base_dir = $self->{config}->{payload_dir} . '/' . $options{token} . '/' . $options{component};
    eval {
        File::Path::make_path($base_dir, { mode => 0750 });
    };
    if ($@ || ! -d $base_dir) {
        return (-1, "cannot create payload directory '$base_dir': $@");
    }

    foreach my $file (@{$options{files}}) {
        my $dest_file = $base_dir . '/' . $file->{name};
        my $dest_dir = $dest_file;
        $dest_dir =~ s/\/[^\/]+$//;
        if ($dest_dir ne $base_dir && ! -d $dest_dir) {
            eval {
                File::Path::make_path($dest_dir, { mode => 0750 });
            };
            if ($@) {
                return (-1, "cannot create payload subdirectory '$dest_dir': $@");
            }
        }

        my $content;
        eval {
            $content = MIME::Base64::decode_base64($file->{content});
        };
        if ($@ || !defined($content)) {
            return (-1, "cannot decode base64 content for file '" . $file->{name} . "'");
        }

        my $tmp_file = $dest_file . '.tmp.' . $$ . '.' . time();
        my $fh;
        if (!open($fh, '>', $tmp_file)) {
            return (-1, "cannot write payload file '$tmp_file': $!");
        }
        binmode($fh);
        print $fh $content;
        close($fh);
        chmod(0640, $tmp_file);
        if (!rename($tmp_file, $dest_file)) {
            unlink($tmp_file);
            return (-1, "cannot finalize payload file '$dest_file': $!");
        }
    }

    return (0, $base_dir);
}

# periodic best-effort cleanup: a payload directory is only ever read once (the
# REMOTECOPY dispatched right after writing it), so anything older than
# payload_max_age is stale (request finished, crashed, or REMOTECOPY never
# consumed it) and safe to remove.
sub purge_old_payloads {
    my ($self, %options) = @_;

    return if ((time() - $self->{payload_purge_interval}) < $self->{payload_purge_last});
    $self->{payload_purge_last} = time();

    my $payload_dir = $self->{config}->{payload_dir};
    return if (! -d $payload_dir);

    my $dh;
    return if (!opendir($dh, $payload_dir));
    my @entries = readdir($dh);
    closedir($dh);

    foreach my $entry (@entries) {
        next if ($entry eq '.' || $entry eq '..');
        my $entry_path = $payload_dir . '/' . $entry;
        next if (! -d $entry_path);

        my @stat = stat($entry_path);
        next if (!@stat || (time() - $stat[9]) < $self->{config}->{payload_max_age});

        eval {
            File::Path::remove_tree($entry_path);
        };
        if ($@) {
            $self->{logger}->writeLogError("[legacycmd] cannot purge stale payload directory '$entry_path': $@");
        } else {
            $self->{logger}->writeLogDebug("[legacycmd] purged stale payload directory '$entry_path'");
        }
    }
}

# --- local execution for the central's own poller (classic VM/RPM install:
# localhost=1, poller_type != 'docker' - gorgone and engine on the same host).
# register_nodes excludes that row, so REMOTECOPY/COMMAND can never route to
# this id through the normal proxy path: copy/exec directly instead of
# dispatching. Not used on docker/k8s TYPE=central, where the central poller
# row is reachable like any other pullwss node (local_poller_id stays undef
# there - see cache_refresh()'s poller_type check).

sub local_copy_path {
    my ($self, %options) = @_;

    if (! -e $options{source}) {
        return (-1, "local copy: source '" . $options{source} . "' does not exist");
    }

    my ($uid, $gid);
    if (defined($options{owner}) && $options{owner} ne '') {
        $uid = getpwnam($options{owner});
        $self->{logger}->writeLogError("[legacycmd] local copy: unknown user '" . $options{owner} . "'") if (!defined($uid));
    }
    if (defined($options{group}) && $options{group} ne '') {
        $gid = getgrnam($options{group});
        $self->{logger}->writeLogError("[legacycmd] local copy: unknown group '" . $options{group} . "'") if (!defined($gid));
    }
    my $mode = defined($options{mode}) ? oct($options{mode}) : undef;

    if (-f $options{source}) {
        my $destination = $options{destination};
        $destination .= File::Basename::basename($options{source}) if ($destination =~ /\/$/);

        eval { File::Path::make_path(File::Basename::dirname($destination)); };
        if (!File::Copy::copy($options{source}, $destination)) {
            return (-1, "local copy: cannot copy '" . $options{source} . "' to '$destination': $!");
        }
        chmod($mode, $destination) if (defined($mode));
        chown($uid // -1, $gid // -1, $destination) if (defined($uid) || defined($gid));
        return 0;
    }

    # directory: mirror the tree (same semantic as the REMOTECOPY archive branch).
    my @inventory = ();
    File::Find::find({ wanted => sub { push @inventory, $File::Find::name if (-f $_); }, no_chdir => 1 }, $options{source});

    foreach my $src_file (@inventory) {
        my $relative = substr($src_file, length($options{source}));
        $relative =~ s/^\///;
        my $dest_file = $options{destination} . '/' . $relative;

        eval { File::Path::make_path(File::Basename::dirname($dest_file)); };
        if (!File::Copy::copy($src_file, $dest_file)) {
            return (-1, "local copy: cannot copy '$src_file' to '$dest_file': $!");
        }
        chmod($mode, $dest_file) if (defined($mode));
        chown($uid // -1, $gid // -1, $dest_file) if (defined($uid) || defined($gid));
    }

    return 0;
}

sub action_centreoncommand {
    my ($self, %options) = @_;

    $self->{logger}->writeLogDebug('[legacycmd] -class- start centreoncommand');
    $options{token} = $self->generate_token() if (!defined($options{token}));
    $self->send_log(code => GORGONE_ACTION_BEGIN, token => $options{token}, data => { message => 'action centreoncommand proceed' });

    if (!defined($options{data}->{content}) || ref($options{data}->{content}) ne 'ARRAY') {
        $self->finalize_batch(token => $options{token}, code => GORGONE_ACTION_FINISH_KO, message => "expected array, found '" . ref($options{data}->{content}) . "'");
        return -1;
    }

    # No-op if already loaded; retries right away if the last periodic refresh failed.
    $self->cache_refresh();
    if ($self->check_pollers_config() == 0) {
        $self->{logger}->writeLogError('[legacycmd] cannot get centreon database configuration');
        $self->finalize_batch(token => $options{token}, code => GORGONE_ACTION_FINISH_KO, message => 'cannot get centreon database configuration');
        return 1;
    }

    # Track only if every command is in %BATCH_SAFE_COMMANDS.
    my $trackable = 1;
    foreach my $command (@{$options{data}->{content}}) {
        $trackable = 0 if (!defined($BATCH_SAFE_COMMANDS{ $command->{command} }));
    }
    $self->start_batch(token => $options{token}) if ($trackable);

    foreach my $command (@{$options{data}->{content}}) {
        my ($code, $message) = $self->execute_cmd(
            action => 1,
            token => $options{token},
            target => $command->{target},
            cmd => $command->{command},
            param => $command->{param},
            files => $command->{files},
            logging => 1
        );

        if ($code == -1) {
            $self->{logger}->writeLogError('[legacycmd] -class- ' . $message);
            $self->finalize_batch(token => $options{token}, code => GORGONE_ACTION_FINISH_KO, message => $message);
            return 1;
        }
    }

    # Tracked: wait on check_pending_batches unless nothing async was dispatched.
    # Untracked: today's behaviour (fire-and-forget) - still close the token.
    if ($trackable) {
        $self->finalize_batch_if_idle(token => $options{token});
    } else {
        $self->finalize_batch(token => $options{token}, code => GORGONE_ACTION_FINISH_OK, message => 'centreoncommand finished successfully');
    }

    $self->{logger}->writeLogDebug('[legacycmd] -class- finish centreoncommand');
    return 0;
}

sub periodic_exec {
    if ($connector->{stop} == 1) {
        $connector->{logger}->writeLogInfo("[legacycmd] $$ has quit");
        exit(0);
    }

    $connector->cache_refresh();
    $connector->handle_cmd_files();
    $connector->purge_old_payloads();
    $connector->check_pending_batches();
}

sub run {
    my ($self, %options) = @_;

    $self->{tpapi_clapi} = gorgone::class::tpapi::clapi->new();
    $self->{tpapi_clapi}->set_configuration(
        config => $self->{tpapi}->get_configuration(name => $self->{tpapi_clapi_name})
    );

    $self->{clapi_user} = $self->{tpapi_clapi}->get_username();
    $self->{clapi_password} = $self->{tpapi_clapi}->get_password(protected => 1);

    # Connect internal
    $self->{internal_socket} = gorgone::standard::library::connect_com(
        context => $self->{zmq_context},
        zmq_type => 'ZMQ_DEALER',
        name => 'gorgone-legacycmd',
        logger => $self->{logger},
        type => $self->get_core_config(name => 'internal_com_type'),
        path => $self->get_core_config(name => 'internal_com_path')
    );
    $self->send_internal_action({
        action => 'LEGACYCMDREADY',
        data => {}
    });

    $self->{db_centreon} = gorgone::class::db->new(
        dsn => $self->{config_db_centreon}->{dsn},
        user => $self->{config_db_centreon}->{username},
        password => $self->{config_db_centreon}->{password},
        force => 2,
        logger => $self->{logger}
    );
    $self->{class_object_centreon} = gorgone::class::sqlquery->new(
        logger => $self->{logger},
        db_centreon => $self->{db_centreon}
    );

    my $watcher_timer = $self->{loop}->timer(5, 5, \&periodic_exec);
    my $watcher_io = $self->{loop}->io($connector->{internal_socket}->get_fd(), EV::READ, sub { $connector->event() } );
    $self->{loop}->run();
}

1;
