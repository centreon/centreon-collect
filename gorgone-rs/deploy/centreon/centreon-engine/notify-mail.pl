#!/usr/bin/perl
# Notification command of the bench: sends one mail over SMTP (Net::SMTP of
# Perl's core libnet), without a shell nor a local mailer, as centreon-engine
# runs its commands with no shell. Usage:
#   notify-mail.pl <smtp host:port> <to> <subject> <body>
use strict;
use warnings;
use Net::SMTP;

my ($server, $to, $subject, $body) = @ARGV;
die "usage: notify-mail.pl server to subject body\n" unless defined $body;
my $smtp = Net::SMTP->new($server, Timeout => 10) or die "$server: $@\n";
$smtp->mail('centreon-engine@bench.local') or die "MAIL FROM refused\n";
$smtp->to($to) or die "RCPT TO $to refused\n";
$smtp->data(
    "From: centreon-engine\@bench.local\r\n",
    "To: $to\r\n",
    "Subject: $subject\r\n",
    "\r\n",
    "$body\r\n",
) or die "DATA refused\n";
$smtp->quit;
