#!/usr/bin/perl
#
# Copyright 2016 Centreon (http://www.centreon.com/)
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

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin";
use centreon::script::centreon_plugins_packaging;

centreon::script::centreon_plugins_packaging->new()->run();

__END__

=head1 NAME

centreon_plugins_packaging - a script to build plugin packages.

=head1 SYNOPSIS

centreon_plugins_packaging.pl [options]

=head1 OPTIONS

=over 8

=item B<--help>

Print a brief help message and exits.

=back

=head1 DESCRIPTION

B<centreon_plugins_packaging> will store trap in a directory (readed by centreontrapd daemon).

=cut

