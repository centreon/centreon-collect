#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long;

my $d = time();
my $dd = localtime();

my $id;
my $state;

GetOptions(
  'id=s'    =>	\$id,
  'state=s' =>	\$state,
);

unless (defined $id) {
  die "'--id' option is mandatory.";
}

{
    use integer;
    $d = ($d + 3 * $id) & 0x1ff;
}

if ($#ARGV gt 1) {
    die "The script must be used with one integer argument\n";
}

my $status = -1;

if ($id eq 0) {
  printf("Host check $dd");
  if (defined $state) {
    $status = $state;
  } else {
    $status = 0;
  }
}
else {
  if (open(FH, '<', "/tmp/states")) {
    while (<FH>) {
      if (/$id=>(.*)/) {
        $status = $1;
        chomp $status;
      }
    }
    close FH;
  }

  $d /= ($id + 1);
  my $w = 300 / ($id + 1);
  my $c = 400 / ($id + 1);
  if ($status == 0) {
    $d = $w / 2;
  } elsif ($status == 1) {
    $d = ($w + $c) / 2;
  } elsif ($status == 2) {
    $d = 2 * $c;
  } else {
    if ($d > $c) {
      $status = 2;
    } elsif ($d > $w) {
      $status = 1;
    } else {
      $status = 0;
    }
  }
  printf("Test check $id | metric=%.2f;%.2f;%.2f\n", $d, $w, $c);
  exit $status;
}
exit $status;
