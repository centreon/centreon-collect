#!/usr/bin/perl
#
# (c)2004 Andreas Wassatsch
# released under GPLv2
#
# based on traceroute.cgi of Ian Cass Knowledge Matters Ltd
# (c)1999 Ian Cass Knowledge Matters Ltd
#
# This script should be put in your Nagios cgi-bin directory
# (usually /usr/local/nagios/sbin)
#
# It will perform a traceroute from your Nagios box to
# the machine that the check_ping plugin is pinging,
# output includes links to host status and status map
# for hosts known to Nagios
#
# This software is provided as-is, without any express or implied 
# warranty. In no event will the author be held liable for any mental 
# or physical damages arising from the use of this script.
#
# Legal note:
# Nagios is a registered trademark of Ethan Galstad.
#
use strict;
use File::Basename;
use POSIX qw(strftime);

# Global Settings
#----------------
$| = 1;
my($nagios)  = "/usr/local/nagios";
my($urlbase) = "/nagios";
my($refresh) = 30;
my($self) = basename($0);
my($traceroute) = "/usr/sbin/traceroute -m 20 -q 1";

# Generate HTTP header
#---------------------
my($mdate)=`date +"%a, %d %b %Y %H:%M:%S %Z"`;
print "Cache-Control: no-store\n";
print "Pragma: no-cache\n";
print "Last-Modified: $mdate";
print "Expires: Thu, 01 Jan 1970 00:00:00 GMT\n";
print "Content-type: text/html\n\n";

# accept either traceroute/foo or traceroute?foo; default to REMOTE_ADDR
# if nothing else is specified
#-----------------------------------------------------------------------
my($addr) = $ENV{PATH_INFO} || $ENV{QUERY_STRING} || $ENV{REMOTE_ADDR};
$addr =~ tr/+/ /;
$addr =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
$addr =~ s,^addr=,,;
$addr =~ s/[^A-Za-z0-9\.-]//g;		# for security


# HTML Head with META Refresh info / stylesheet
#----------------------------------------------
print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n";
print "<html>\n<head>\n<title>traceroute to host $addr</title>\n";
print "<meta http-equiv='refresh' content='$refresh;url=$self?$addr'>\n";
print "<link rel='stylesheet' type='text/css' href='$urlbase/stylesheets/status.css'>\n";
print "</head>\n<body>\n";

# Info Box
#---------
print "<table class='infoBox' border=1 cellspacing=0 cellpadding=0>";
print "<tr><td class='infoBox'>";
print "<div class='infoBoxTitle'>traceroute</div>";
print "Genereated by $self<br>";
print "Last Updated: $mdate<br>";
print "Updated every $refresh seconds<br>";
print "Nagios&reg; - <a href='http://www.nagios.org' target='_new' class='homepageURL'>www.nagios.org</a><br>";
print "Logged in as <i>$ENV{'REMOTE_USER'}</i><br>";
print "</td></tr>";
print "</table>";

print "<p><div align=center class='statusTitle'>";
print "Traceroute to Host $addr</div><p>\n";

print "<table border=0 class='status'>\n";

# read in nagios hosts
#---------------------
my(@cfg);
my($entry);
my($bla);
my($host);
my(@hostlist);
open(HOSTS, "$nagios/etc/hosts.cfg");
    @cfg = grep {!/#/ && /host_name/} <HOSTS>;
close(HOSTS);

foreach $entry (@cfg) {
    $entry =~ s/^\s+//;
    ($bla, $host) = split(/\s+/,$entry);
    push @hostlist,$host;
}

# open traceroute pipe
#---------------------
my($i)=0;
open(TRACEROUTE, "$traceroute $addr |" ) ||
    die "<pre>couldn't open pipe to traceroute! $!</pre>";

my(@arr);
my($class);
my($known_host);
while (<TRACEROUTE>) {
    chomp;
    s/\&/\&amp;/g;
    s/\</\&lt;/g;

    if ($i == 0) {

	# print table header
	#-------------------
        print "<tr>";
  	print "<th class='status'>Hop</th>";
  	print "<th class='status'>Host</th>";
  	print "<th class='status'>IP</th>";
  	print "<th class='status'>Round Trip Time</th>";
	print "</tr>\n";

    } else {

	# class for odd/even lines
	#-------------------------
        if ($i/2 == int($i/2)) {
	    $class="statusEven";
        } else {
	    $class="statusOdd";
        }	

	# parse traceroute lines
	#-----------------------
        s/^\s//g;
    	(@arr) = split(/\s+/, $_, 4);
        if (grep(/\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/,$arr[1])) {
	   $arr[3] = $arr[2] ." ". $arr[3];
	   $arr[2] = $arr[1];
	   $arr[1] = "-";
	}
        $arr[2] =~ s/\(//; 
  	$arr[2] =~ s/\)//;

	# check if host is known to nagios
	#---------------------------------
	$known_host = 0;
	foreach $host (@hostlist) {
	    if ($host eq $arr[1]) {
		$known_host++;
	    }
	}

	# print table row
	#----------------	
        print "<tr>";
 	print "<td class='$class'>$arr[0]</td>\n"; # hop

 	print "<td class='$class'>";
	print "<table width=100%><tr>\n";
	print "<td align=left class='$class'>\n";

	if ($known_host) {
	    print "<a href='$urlbase/cgi-bin/extinfo.cgi?type=1&host=$arr[1]'>";
	    print "$arr[1]</a></td>\n";
	} else {
	    print "$arr[1]</td>\n";
	}

	print "<td width=25 align=right class='$class'>\n";
	if ($known_host) {
	    print "<a href=statusmap.cgi?host=$arr[1]>";
	    print "<img align=top align=right border=0 width=20 height=20 ";
	    print "src='$urlbase/images/status3.gif'></a>\n";
	}
	print "</td></tr></table>\n"; # host

 	print "<td class='$class'>$arr[2]</td>\n"; # ip
 	print "<td class='$class'>$arr[3]</td>\n"; # rtt
	print "</tr>\n";
    }

    $i++;
}
close(TRACEROUTE) || die "couldn't close pipe to traceroute! $!";

print "</table>\n";


# footer
#-------
print "&nbsp;<p>&nbsp;<p>";
print "<font size=-2><i>$self by Andreas Wassatsch</i></font>";
print "</body></html>\n";

# end
#----
exit 0;


