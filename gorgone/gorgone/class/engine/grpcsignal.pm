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

package gorgone::class::engine::grpcsignal;

use strict;
use warnings;
use gorgone::standard::misc;
use IO::Socket::INET;
use IO::Select;

# Minimal unary gRPC-over-HTTP/2-cleartext (prior-knowledge) client for a
# SignalProcess-shaped RPC (one enum field, RESTART/RELOAD = 0, SHUTDOWN = 1,
# wire-identical regardless of the proto package, so 'path' picks the real
# target) - not a general gRPC client. Native Perl port of the formerly-
# shimmed .github/docker/centreon-gorgone/trixie/grpc-signal.pl script (same
# wire format, same Protocol::HTTP2::Client library), so a RELOAD or RESTART
# dispatched to a node no longer needs to shell out to systemctl and spawn
# that external script. Protocol::HTTP2::Client stays an optional,
# lazily-loaded dependency: most gorgone installs (RPM/DEB) never call this.
sub signal_process {
    my (%options) = @_;

    if (gorgone::standard::misc::mymodule_load(
        logger => $options{logger},
        module => 'Protocol::HTTP2::Client',
        error_msg => "Cannot load module 'Protocol::HTTP2::Client'. gRPC signal unavailable."
    ) == 1) {
        return (-1, "Protocol::HTTP2::Client not available");
    }

    # process: RESTART/RELOAD = 0 (config reload in place, process stays up),
    # SHUTDOWN = 1 (stop the process - a container/orchestrator restart
    # policy is expected to bring it back). Proto3 field 1 varint: value 0 is
    # the wire default and is omitted entirely (empty message); value 1 is
    # encoded as the two raw bytes 0x08 0x01.
    my $body = $options{signal} eq 'restart' ? pack('H*', '0801') : '';
    my $grpc_body = pack('C N', 0, length($body)) . $body;

    my $got_status;
    my $got_error;

    my $h2 = Protocol::HTTP2::Client->new->request(
        ':method'    => 'POST',
        ':path'      => $options{path},
        ':scheme'    => 'http',
        ':authority' => $options{host} . ':' . $options{port},
        headers      => [
            'content-type' => 'application/grpc',
            'te'           => 'trailers'
        ],
        data       => $grpc_body,
        on_headers => sub {
            my $headers = shift;
            my %h = @$headers;
            $got_status = $h{':status'};
            return 1;
        },
        on_error => sub {
            $got_error = shift;
        }
    );

    my $sock = IO::Socket::INET->new(
        PeerHost => $options{host},
        PeerPort => $options{port},
        Proto    => 'tcp',
        Timeout  => 5
    );
    if (!$sock) {
        return (-1, "connect to " . $options{host} . ':' . $options{port} . " failed: $!");
    }
    $sock->blocking(0);
    my $sel = IO::Select->new($sock);

    # Deadline-based instead of alarm()/SIGALRM: this runs inside a long-lived
    # gorgone-action child, alarm() is process-global and would risk firing
    # across unrelated code elsewhere in that process.
    my $timeout = defined($options{timeout}) ? $options{timeout} : 10;
    my $deadline = time() + $timeout;

    while (!defined($got_status) && !defined($got_error) && time() < $deadline) {
        if ($sel->can_write(1)) {
            while (my $frame = $h2->next_frame) {
                syswrite($sock, $frame);
            }
        }
        if ($sel->can_read(1)) {
            my $n = sysread($sock, my $data, 4096);
            last if (!defined($n) || $n == 0);
            $h2->feed($data);
        }
    }
    $sock->close();

    if (!defined($got_status) && !defined($got_error)) {
        return (-1, "timeout waiting for gRPC response from " . $options{host} . ':' . $options{port});
    }
    if (defined($got_error)) {
        return (-1, "gRPC error: $got_error");
    }
    if ($got_status ne '200') {
        return (-1, "gRPC call failed: HTTP status = $got_status");
    }

    return 0;
}

1;

__END__

=head1 NAME

gorgone::class::engine::grpcsignal

=head1 SYNOPSIS

Sends a SignalProcess-shaped gRPC call (reload or restart) without shelling
out, to a caller-chosen RPC C<path> (e.g. centengine's
C<Engine/SignalProcess>). Used by gorgone::modules::core::action::class when
the C<action> module's C<engine_grpc.enable> config is set.

=cut
