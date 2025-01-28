#!/usr/bin/perl
#
# Copyright 2023-2024 Centreon
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
# For more information : contact@centreon.com
#
# This script is a little tcp server working on port 5669. It can simulate
# a cbd instance. It is useful to test the validity of BBDO packets sent by
# centengine.
use strict;
use warnings;
use Getopt::Long;

my $d = time();
my $dd = localtime();

my $id;
my $state;
my $output;

GetOptions(
  'id=s'    =>	\$id,
  'state=s' =>	\$state,
  'output=s' => \$output,
);

unless (defined $id) {
  die "'--id' option is mandatory.";
}

{
    use integer;
    $d = ($d + 3 * $id) & 0x1ff;
}

my $status = -1;

if ($id eq 0) {
  if (defined $output) {
    printf("Host check $dd: $output\n");
  } else {
    printf("Host check $dd\n");
  }
  if (defined $state) {
    $status = $state;
  } else {
    $status = 0;
  }
}
else {
  if (open(FH, '<', "/tmp/states")) {
    while (<FH>) {
      if (/^$id=>(.*)/) {
        $status = $1;
        chomp $status;
	last;
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
