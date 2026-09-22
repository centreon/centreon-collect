#!/usr/bin/perl
# Minimal generic unary gRPC-over-HTTP/2-cleartext (prior-knowledge) client.
# Usage: grpc-signal.pl <host> <port> </package.Service/Method> [<hex-request-body>]
# Exits 0 if the server answered with HTTP :status 200 on this stream, 1 otherwise.
#
# Replaces shelling out to the "buf" Go binary for the single narrow RPC this
# image needs (centengine's Engine/SignalProcess) - avoids pulling in a full
# Go toolchain's worth of CVE surface for one unary call with a tiny message.
# Verified against a real centengine gRPC server: RESTART reloads config,
# SHUTDOWN stops the process, connection/timeout failures exit non-zero.
use strict;
use warnings;
use Protocol::HTTP2::Client;
use IO::Socket::INET;
use IO::Select;

my ($host, $port, $path, $hex_body) = @ARGV;
die "usage: $0 <host> <port> </pkg.Service/Method> [<hex-request-body>]\n"
    unless defined $host && defined $port && defined $path;

my $body = defined $hex_body ? pack('H*', $hex_body) : '';
# gRPC message framing: 1-byte compressed flag + 4-byte big-endian length + payload
my $grpc_body = pack('C N', 0, length $body) . $body;

my $got_status;
my $got_error;

my $h2 = Protocol::HTTP2::Client->new->request(
    ':method'    => 'POST',
    ':path'      => $path,
    ':scheme'    => 'http',
    ':authority' => "$host:$port",
    headers      => [
        'content-type' => 'application/grpc',
        'te'           => 'trailers',
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
    },
);

my $sock = IO::Socket::INET->new(
    PeerHost => $host,
    PeerPort => $port,
    Proto    => 'tcp',
    Timeout  => 5,
);
if (!$sock) {
    print STDERR "connect to $host:$port failed: $!\n";
    exit 1;
}
$sock->blocking(0);
my $sel = IO::Select->new($sock);

my $timed_out = 0;
local $SIG{ALRM} = sub { $timed_out = 1 };
alarm(10);

while (!defined $got_status && !defined $got_error && !$timed_out) {
    if ($sel->can_write(1)) {
        while (my $frame = $h2->next_frame) {
            syswrite $sock, $frame;
        }
    }
    if ($sel->can_read(1)) {
        my $n = sysread $sock, my $data, 4096;
        last if !defined $n || $n == 0;
        $h2->feed($data);
    }
}
alarm(0);

if ($timed_out) {
    print STDERR "timeout waiting for gRPC response from $host:$port\n";
    exit 1;
}

if (defined $got_error) {
    print STDERR "gRPC error: $got_error\n";
    exit 1;
}
if (!defined $got_status || $got_status ne '200') {
    print STDERR "gRPC call failed: HTTP status = " . ($got_status // 'none') . "\n";
    exit 1;
}
exit 0;
