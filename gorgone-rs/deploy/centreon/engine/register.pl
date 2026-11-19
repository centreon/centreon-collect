#!/usr/bin/perl
# Provisioning of an engine container of the bench, run by its entrypoint.
# Core Perl only (HTTP::Tiny, JSON::PP): the image has no Python and no curl.
#
# register: declares the poller in centreon-web when needed, removes its
#           command file (see no_command_file) and prints the id web gave it,
#           which is the node id of its gorgone-agent.
# objects:  creates the objects of the CLAPI_OBJECTS file (centreon-engine).
# deploy:   removes the command file of the poller (the Central poller is not
#           registered by this script), waits for gorgone-server to see the
#           agent, then asks centreon-web to generate, push and restart, as an
#           operator would.
#
# Only the network is used: no file is shared with another container.

use strict;
use warnings;
use HTTP::Tiny;
use JSON::PP ();
use MIME::Base64 ();

my $WEB = 'http://' . ($ENV{WEB_HOST} // 'web');
my $GORGONE = sprintf('http://%s:%s',
    $ENV{GORGONE_HOST} // 'gorgone-server', $ENV{GORGONE_API_PORT} // '8085');
my $POLLER = $ENV{POLLER_NAME} // 'engine';
# Path of the module in the configuration centreon-web generates.
my $EXTERNALCMD = '/usr/lib64/centreon-engine/externalcmd.so';

my $JSON = JSON::PP->new->utf8->canonical;
$| = 1;

# Sends a JSON request; an empty answer gives an empty hash. Dies on a
# network error or an HTTP error status, with the status in the message.
sub call {
    my ($url, $payload, $headers, $timeout) = @_;
    my %options = (headers => {%{$headers // {}}});
    if (defined $payload) {
        $options{content} = $JSON->encode($payload);
        $options{headers}{'Content-Type'} = 'application/json';
    }
    my $http = HTTP::Tiny->new(timeout => $timeout // 60);
    my $res = $http->request(defined $payload ? 'POST' : 'GET', $url, \%options);
    if (!$res->{success}) {
        my $detail = $res->{status} == 599 ? $res->{content} : $res->{reason};
        chomp($detail);
        die { status => $res->{status}, message => "$url: $res->{status} $detail" };
    }
    return $res->{content} =~ /\S/ ? $JSON->decode($res->{content}) : {};
}

sub error_text {
    my ($error) = @_;
    return ref($error) eq 'HASH' ? $error->{message} : "$error" =~ s/\s+$//r;
}

# Logs in, waiting for centreon-web to be up and warm.
sub login {
    my %credentials = (
        login => $ENV{WEB_API_USERNAME} // 'admin',
        password => $ENV{WEB_API_PASSWORD} // 'Centreon!2021',
    );
    for my $attempt (0 .. 179) {
        my $token = eval {
            my $answer = call("$WEB/centreon/api/latest/login",
                {security => {credentials => \%credentials}});
            $answer->{security}{token} // die "no token in the login answer\n";
        };
        return $token if defined $token;
        die $@ if $attempt == 179;
        print STDERR 'centreon-web not ready yet: ', error_text($@), "\n";
        sleep 5;
    }
}

sub poller_id {
    my ($token) = @_;
    # Same text as Python's json.dumps({"name": POLLER}).
    my $search = sprintf('{"name": %s}', JSON::PP->new->ascii->allow_nonref->encode($POLLER));
    $search =~ s/([^A-Za-z0-9_.~\/-])/sprintf('%%%02X', ord($1))/ge;
    my $answer = call("$WEB/centreon/api/latest/configuration/monitoring-servers?search=$search",
        undef, {'X-AUTH-TOKEN' => $token});
    return @{$answer->{result}} ? $answer->{result}[0]{id} : undef;
}

sub clapi {
    my ($token, %request) = @_;
    return call("$WEB/centreon/api/index.php?action=action&object=centreon_clapi",
        \%request, {'centreon-auth-token' => $token});
}

# The engine of this container is driven through its gRPC API only: no
# externalcmd.so module (not in the image) and no command_file line in the
# Engine configuration of the poller, so nothing reads or creates the FIFO.
sub no_command_file {
    my ($token) = @_;
    my @configs = grep { $_->{instance} eq $POLLER }
        @{clapi($token, action => 'show', object => 'ENGINECFG')->{result}};
    die "no Engine configuration for poller $POLLER\n" unless @configs;
    for my $config (@configs) {
        my $name = $config->{'nagios name'};
        eval {
            clapi($token, action => 'delbrokermodule', object => 'ENGINECFG',
                values => "$name;$EXTERNALCMD");
            1;
        } or do {
            # 404: no such module line, already removed.
            die $@ unless ref($@) eq 'HASH' && $@->{status} == 404;
        };
        # An empty value stores NULL: centreon-web then writes no command_file.
        clapi($token, action => 'setparam', object => 'ENGINECFG',
            values => "$name;command_file;");
        print STDERR "Engine configuration '$name': no $EXTERNALCMD, no command_file\n";
    }
}

sub register {
    my $token = login();
    if (!defined poller_id($token)) {
        call("$WEB/centreon/api/latest/platform/topology",
            {name => $POLLER, hostname => $POLLER, type => 'poller', address => $POLLER,
             parent_address => '127.0.0.1'},
            {'X-AUTH-TOKEN' => $token});
        call("$WEB/centreon/api/index.php?object=centreon_configuration_remote"
                . '&action=linkCentreonRemoteServer',
            {linked_remote_master => '', linked_remote_slaves => [],
             open_broker_flow => JSON::PP::false,
             centreon_central_ip => $ENV{BROKER_HOST} // 'broker', server_ip => $POLLER,
             server_name => $POLLER, server_type => 'poller'},
            {'centreon-auth-token' => $token});
    }
    my $node_id = poller_id($token);
    die "centreon-web did not create poller $POLLER\n" unless defined $node_id;
    no_command_file($token);
    print "$node_id\n";
}

# Waits until every URL of WAIT_FOR_URLS answers, when the variable is set.
sub wait_for_urls {
    URL: for my $url (split ' ', $ENV{WAIT_FOR_URLS} // '') {
        for (1 .. 180) {
            next URL if HTTP::Tiny->new(timeout => 5)->get($url)->{success};
            sleep 1;
        }
        die "no answer from $url\n";
    }
}

sub deploy {
    my $token = login();
    my $node_id = poller_id($token) // 'None';
    no_command_file($token);
    for my $variable (qw(GORGONE_API_USERNAME GORGONE_API_PASSWORD)) {
        die "$variable is not set\n" unless defined $ENV{$variable};
    }
    my $basic = MIME::Base64::encode_base64(
        "$ENV{GORGONE_API_USERNAME}:$ENV{GORGONE_API_PASSWORD}", '');
    my $seen = 0;
    for (1 .. 120) {
        my $nodes = call("$GORGONE/v1/nodes", undef, {Authorization => "Basic $basic"})->{nodes};
        if (grep { $_->{id} eq $node_id && $_->{connected} } @$nodes) {
            $seen = 1;
            last;
        }
        sleep 1;
    }
    die "gorgone-server never saw the agent of node $node_id\n" unless $seen;
    for my $action (qw(POLLERGENERATE CFGMOVE POLLERRESTART)) {
        if ($action eq 'POLLERRESTART') {
            wait_for_urls();
            # The restart may be this container's: note the deployment first,
            # or the next start would deploy, and restart, again.
            if (my $marker = $ENV{DEPLOY_MARKER}) {
                open(my $fh, '>', $marker) or die "$marker: $!\n";
                close($fh);
            }
        }
        my $answer = clapi($token, action => $action, values => $POLLER);
        print "$action ", $JSON->encode($answer), "\n";
    }
}

# Creates the objects of the CLAPI_OBJECTS file (OBJECT;ACTION;values lines,
# @POLLER@ standing for the poller name). An ADD of an object that already
# exists is skipped: the container may start again.
sub objects {
    my $file = $ENV{CLAPI_OBJECTS} // die "CLAPI_OBJECTS is not set\n";
    my $token = login();
    open(my $fh, '<', $file) or die "$file: $!\n";
    while (my $line = <$fh>) {
        chomp($line);
        next if $line =~ /^\s*(#|$)/;
        $line =~ s/\@POLLER\@/$POLLER/g;
        my ($object, $action, $values) = split(/;/, $line, 3);
        my $done = eval {
            clapi($token, object => $object, action => $action, values => $values // '');
            1;
        };
        next if $done;
        # CLAPI answers 409 for an object that already exists.
        die $@ unless $action eq 'ADD' && ref($@) eq 'HASH' && $@->{status} == 409;
        print STDERR "$object $values: already there\n";
    }
    close($fh);
}

my %commands = (register => \&register, deploy => \&deploy, objects => \&objects);
my $command = $commands{$ARGV[0] // ''};
if (!$command) {
    print STDERR "Usage: register.pl register|deploy|objects\n";
    exit 1;
}
if (!eval { $command->(); 1 }) {
    print STDERR error_text($@), "\n";
    exit 1;
}
