/*
** Copyright 2005-2008 Nagios Plugins Development Team
** Copyright 2011      Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU General Public License
** version 2 as published by the Free Software Foundation.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <exception>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <ctype.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <float.h>
#include "com/centreon/connector/icmp/check.hh"

using namespace com::centreon::connector::icmp;

float const check::_pkt_backoff_factor(1.5);
float const check::_target_backoff_factor(1.5);

check::check(unsigned long cmd_id, QStringList const& arguments, unsigned int timeout)
  : _out(&_out_buffer),
    _arguments(arguments),
    _sock_manager(socket_manager::instance()),
    _max_completion_time(0),
    _cmd_id(cmd_id),
    _debug(0),
    _icmp_sent(0),
    _icmp_recv(0),
    _icmp_lost(0),
    _pkt_interval(80000),
    _timeout(timeout),
    _min_hosts_alive(-1),
    _mode(RTA),
    _icmp_sock(0),
    _target_interval(0),
    _exit_code(OK),
    _pid(getpid() & 0xFFFF),
    _targets_down(0),
    _targets(0),
    _packets(5),
    _icmp_data_size(DEFAULT_PING_DATA_SIZE),
    _icmp_pkt_size(DEFAULT_PING_DATA_SIZE + ICMP_MINLEN),
    _ttl(0),
    _is_running(false) {
  _out.setRealNumberPrecision(3);
  _out.setRealNumberNotation(QTextStream::FixedNotation);

  _crit.rta = 500000;
  _crit.pl = 80;
  _warn.rta = 200000;
  _warn.pl = 40;

  if (QCoreApplication::applicationName() == "connector_icmp"
      || QCoreApplication::applicationName() == "connector_ping") {
    _mode = ICMP;
  }
  else if (QCoreApplication::applicationName() == "connector_host") {
    _mode = HOSTCHECK;
    _pkt_interval = 1000000;
    _crit.rta = 1000000;
    _crit.pl = 100;
    _warn.rta = 1000000;
    _warn.pl = 100;
  }
  else if (QCoreApplication::applicationName() == "connector_rta_multi") {
    _mode = ALL;
    _target_interval = 0;
    _pkt_interval = 50000;
  }
}

check::~check() throw() {

}

unsigned long check::get_command_id() const throw() {
  return (_cmd_id);
}

QString const& check::get_output() {
  _out.flush();
  return (_out_buffer);
}

int check::get_exit_code() const throw() {
  return (_exit_code);
}

void check::run() {
  emit start();

  try {
    _icmp_sock = _sock_manager.take();
    if (!_parse_args()) {
      _exit_code = UNKNOWN;
      _sock_manager.release(_icmp_sock);
      emit finish(_cmd_id, get_output(), _exit_code);
      return;
    }

    /* make sure we don't wait any longer than necessary */
    gettimeofday(&_prog_start, NULL);
    _max_completion_time =
      ((_targets * _packets * _pkt_interval) + (_targets * _target_interval))
      + (_targets * _packets * _crit.rta) + _crit.rta;

    if (_debug) {
      _out << "packets: " << _packets << ", targets: " << _targets << "\n"
	     << "target_interval: " << (float)_target_interval / 1000
	     << ", pkt_interval " << (float)_pkt_interval / 1000 << "\n"
	     << "crit.rta: " << (float)_crit.rta / 1000 << "\n"
	     << "max_completion_time: " << (float)_max_completion_time / 1000 << "\n";
      _out << "crit = {" << _crit.rta << ", " << _crit.pl << "%}, "
	   << "warn = {" << _warn.rta << ", " << _warn.pl << "%}\n";
      _out << "pkt_interval: " << _pkt_interval
	   << "  target_interval: " << _target_interval << "\n";
      _out << "icmp_pkt_size: " << _icmp_pkt_size << "\n";
    }

    for (unsigned int i = 0, end = _list_host.size(); i < end; ++i)
      _list_host[i]->id = i * _packets;

    _exit_code = (_run_checks() < 0 ? UNKNOWN : _finish());
  }
  catch (std::exception const& e) {
    _exit_code = UNKNOWN;
    _out << "exception: " << e.what();
  }

  _sock_manager.release(_icmp_sock);
  if (_get_timevaldiff(&_prog_start, NULL) < _timeout)
    emit finish(_cmd_id, get_output(), _exit_code);
}

bool check::_parse_args() {
  if (_arguments.size() < 2)
    return (false);

  QStringList::const_iterator it = _arguments.begin() + 1;
  QStringList::const_iterator end = _arguments.end();
  while (it != end && (*it)[0] == '-') {
    QString key = *it;

    if (key == "-v")
      ++_debug;
    else if (key == "-h") {
      _print_help();
      return (false);
    }

    if (++it == end) {
      _out << QCoreApplication::applicationName() << ": bad argument.";
      return (false);
    }

    else if (key == "-H") {
      if (_add_target(qPrintable(*it)) == -1)
	return (false);
    }
    else if (key == "-b") {
      long size = it->toLong();
      if (size >= static_cast<long>(sizeof(::icmp) + sizeof(icmp_ping_data))
	  && size < MAX_PING_DATA) {
      	_icmp_data_size = size;
      	_icmp_pkt_size = size + ICMP_MINLEN;
      }
      else {
      	_out << QCoreApplication::applicationName()
	     << ": ICMP data length must be between: "
	     << sizeof(::icmp) + sizeof(icmp_ping_data)
	     << " and " << MAX_PING_DATA - 1 << "\n";
      	return (false);
      }
    }
    else if (key == "-i")
      _pkt_interval = _get_timevar(qPrintable(*it));
    else if (key == "-I")
      _target_interval = _get_timevar(qPrintable(*it));
    else if (key == "-w")
      _get_threshold(qPrintable(*it), &_warn);
    else if (key == "-c")
      _get_threshold(qPrintable(*it), &_crit);
    else if (key == "-n" || key == "-p")
      _packets = it->toShort();
    else if (key == "-l")
      _ttl = static_cast<unsigned char>(it->toUInt());
    else if (key == "-m")
      _min_hosts_alive = it->toInt();
    else if (key == "-s") {
      if (!_set_source_ip(qPrintable(*it)))
	return (false);
    }
    ++it;
  }

  while (it != end) {
    if (_add_target(qPrintable(*it)) == -1)
      return (false);
    ++it;
  }

  if (!_targets) {
    _out << QCoreApplication::applicationName() << ": No hosts to check\n";
    return (false);
  }

  if (_packets > 20) {
    _out << QCoreApplication::applicationName() << ": packets is > 20 (" << _packets << ")\n";
    return (false);
  }

  if (_min_hosts_alive < -1) {
    _out << QCoreApplication::applicationName() << ": minimum alive hosts is negative (" << _min_hosts_alive << ")\n";
    return (false);
  }

  if (!_ttl)
    _ttl = 64;

  int result = setsockopt(_icmp_sock, SOL_IP, IP_TTL, &_ttl, sizeof(_ttl));
  if (_debug) {
    if (result == -1)
      _out << "setsockopt failed\n";
    else
      _out << "ttl set to " << static_cast<unsigned int>(_ttl) << "\n";
  }

  /*
   * stupid users should be able to give whatever thresholds they want
   * (nothing will break if they do), but some anal plugin maintainer
   * will probably add some printf() thing here later, so it might be
   * best to at least show them where to do it. ;)
   */
  if (_warn.pl > _crit.pl)
    _warn.pl = _crit.pl;
  if (_warn.rta > _crit.rta)
    _warn.rta = _crit.rta;

  return (true);
}

char const* check::_get_icmp_error_msg(unsigned char icmp_type, unsigned char icmp_code) {
  char const* msg = "unreachable";

  if (_debug > 1)
    _out << "get_icmp_error_msg(" << icmp_type << ", " << icmp_code << ")\n";

  switch (icmp_type) {
  case ICMP_UNREACH:
    switch (icmp_code) {
    case ICMP_UNREACH_NET: msg = "Net unreachable"; break;
    case ICMP_UNREACH_HOST:	msg = "Host unreachable"; break;
    case ICMP_UNREACH_PROTOCOL: msg = "Protocol unreachable (firewall?)"; break;
    case ICMP_UNREACH_PORT: msg = "Port unreachable (firewall?)"; break;
    case ICMP_UNREACH_NEEDFRAG: msg = "Fragmentation needed"; break;
    case ICMP_UNREACH_SRCFAIL: msg = "Source route failed"; break;
    case ICMP_UNREACH_ISOLATED: msg = "Source host isolated"; break;
    case ICMP_UNREACH_NET_UNKNOWN: msg = "Unknown network"; break;
    case ICMP_UNREACH_HOST_UNKNOWN: msg = "Unknown host"; break;
    case ICMP_UNREACH_NET_PROHIB: msg = "Network denied (firewall?)"; break;
    case ICMP_UNREACH_HOST_PROHIB: msg = "Host denied (firewall?)"; break;
    case ICMP_UNREACH_TOSNET: msg = "Bad TOS for network (firewall?)"; break;
    case ICMP_UNREACH_TOSHOST: msg = "Bad TOS for host (firewall?)"; break;
    case ICMP_UNREACH_FILTER_PROHIB: msg = "Prohibited by filter (firewall)"; break;
    case ICMP_UNREACH_HOST_PRECEDENCE: msg = "Host precedence violation"; break;
    case ICMP_UNREACH_PRECEDENCE_CUTOFF: msg = "Precedence cutoff"; break;
    default: msg = "Invalid code"; break;
    }
    break;

  case ICMP_TIMXCEED:
    /* really 'out of reach', or non-existant host behind a router serving
     * two different subnets */
    switch (icmp_code) {
    case ICMP_TIMXCEED_INTRANS: msg = "Time to live exceeded in transit"; break;
    case ICMP_TIMXCEED_REASS: msg = "Fragment reassembly time exceeded"; break;
    default: msg = "Invalid code"; break;
    }
    break;

  case ICMP_SOURCEQUENCH: msg = "Transmitting too fast"; break;
  case ICMP_REDIRECT: msg = "Redirect (change route)"; break;
  case ICMP_PARAMPROB: msg = "Bad IP header (required option absent)"; break;

    /* the following aren't error messages, so ignore */
  case ICMP_TSTAMP:
  case ICMP_TSTAMPREPLY:
  case ICMP_IREQ:
  case ICMP_IREQREPLY:
  case ICMP_MASKREQ:
  case ICMP_MASKREPLY:
  default: msg = ""; break;
  }

  return (msg);
}

int check::_handle_random_icmp(unsigned char const* packet, sockaddr_in const* addr) {
  ::icmp p;
  memcpy(&p, packet, sizeof(p));
  if (p.icmp_type == ICMP_ECHO && ntohs(p.icmp_id) == _pid) {
    /* echo request from us to us (pinging localhost) */
    return (0);
  }

  if (_debug)
    _out << "handle_random_icmp(" << static_cast<void const*>(&p)
	 << ", " << static_cast<void const*>(addr) << ")\n";

  /* only handle a few types, since others can't possibly be replies to
   * us in a sane network (if it is anyway, it will be counted as lost
   * at summary time, but not as quickly as a proper response */
  /* TIMXCEED can be an unreach from a router with multiple IP's which
   * serves two different subnets on the same interface and a dead host
   * on one net is pinged from the other. The router will respond to
   * itself and thus set TTL=0 so as to not loop forever.  Even when
   * TIMXCEED actually sends a proper icmp response we will have passed
   * too many hops to have a hope of reaching it later, in which case it
   * indicates overconfidence in the network, poor routing or both. */
  if (p.icmp_type != ICMP_UNREACH && p.icmp_type != ICMP_TIMXCEED &&
      p.icmp_type != ICMP_SOURCEQUENCH && p.icmp_type != ICMP_PARAMPROB)
    return (0);

  /* might be for us. At least it holds the original package (according
   * to RFC 792). If it isn't, just ignore it */
  ::icmp sent_icmp;
  memcpy(&sent_icmp, packet + 28, sizeof(sent_icmp));
  if (sent_icmp.icmp_type != ICMP_ECHO || ntohs(sent_icmp.icmp_id) != _pid
      || ntohs(sent_icmp.icmp_seq) >= _targets * _packets) {
    if (_debug)
      _out << "Packet is no response to a packet we sent\n";
    return (0);
  }

  /* it is indeed a response for us */
  rta_host* host = _list_host[ntohs(sent_icmp.icmp_seq) / _packets];
  if (_debug)
    _out << "Received \"" << _get_icmp_error_msg(p.icmp_type, p.icmp_code)
	 << "\" from " << inet_ntoa(addr->sin_addr)
	 << " for ICMP ECHO sent to " << host->name << ".\n";

  _icmp_lost++;
  host->icmp_lost++;
  /* don't spend time on lost hosts any more */
  if (host->flags & FLAG_LOST_CAUSE)
    return (0);

  /* source quench means we're sending too fast, so increase the
   * interval and mark this packet lost */
  if (p.icmp_type == ICMP_SOURCEQUENCH) {
    _pkt_interval *= _pkt_backoff_factor;
    _target_interval *= _target_backoff_factor;
  }
  else {
    _targets_down++;
    host->flags |= FLAG_LOST_CAUSE;
  }
  host->icmp_type = p.icmp_type;
  host->icmp_code = p.icmp_code;
  host->error_addr.s_addr = addr->sin_addr.s_addr;

  return (0);
}

void check::_print_help(void) {
  _out << "Usage:\n";
  _out << " " << QCoreApplication::applicationName() << " [options] [-H] host1 host2 hostN\n";

  _out << " -h\n";
  _out << "    Print detailed help screen\n";
  _out << " -H\n";
  _out << "    specify a target\n";
  _out << " -w\n";
  _out << "    warning threshold (currently " << (float)_warn.rta / 1000 << "ms,"
       << _warn.pl << "%\n";
  _out << " -c\n";
  _out << "    critical threshold (currently " << (float)_crit.rta / 1000 << "ms,"
       << _crit.pl << "%\n";
  _out << " -s\n";
  _out << "    specify a source IP address or device name";
  _out << " -n\n";
  _out << "    number of _packets to send (currently " << _packets << ")\n";
  _out << " -i\n";
  _out << "    max packet interval (currently " << (float)_pkt_interval / 1000 << "ms)\n";
  _out << " -I\n";
  _out << "    max target interval (currently " << (float)_target_interval / 1000 << "ms)\n";
  _out << " -m\n";
  _out << "    number of alive hosts required for success\n";
  _out << " -l\n";
  _out << "    TTL on outgoing _packets (currently " << static_cast<unsigned int>(_ttl) << ")\n";
  _out << " -t\n";
  _out << " -b\n";
  _out << "    Number of icmp data bytes to send\n";
  _out << "    Packet size will be data bytes + icmp header (currently " << _icmp_data_size
       << " + " << ICMP_MINLEN << ")\n";
  _out << " -v\n";
  _out << "    verbose\n";

  _out << "\n";
  _out << "Notes:\n";
  _out << " The -H switch is optional. Naming a host (or several) to check is not.\n";
  _out << "\n";
  _out << " Threshold format for -w and -c is 200.25,60% for 200.25 msec RTA and 60%\n";
  _out << " packet loss.  The default values should work well for most users.\n";
  _out << " You can specify different RTA factors using the standardized abbreviations\n";
  _out << " us (microseconds), ms (milliseconds, default) or just plain s for seconds.\n";
  _out << "\n";
  _out << " The -v switch can be specified several times for increased verbosity.\n";
  /*  _out << "Long options are currently unsupported.\n";
      _out << "Options marked with * require an argument\n";
  */
}

/*
 * u = micro
 * m = milli
 * s = seconds
 * return value is in microseconds
 */
unsigned int check::_get_timevar(char const* str) {
  if (!str)
    return (0);
  unsigned int len = strlen(str);
  if (!len)
    return (0);

  /* unit might be given as ms|m (millisec),
   * us|u (microsec) or just plain s, for seconds */
  char p = 0;
  char u = str[len - 1];
  if (len >= 2 && !isdigit((int)str[len - 2]))
    p = str[len - 2];
  if (p && u == 's')
    u = p;
  else if (!p)
    p = u;
  if (_debug > 2)
    _out << "evaluating " << str << ", u: " << u << ", p: " << p << "\n";

  unsigned int factor = 1000; /* default to milliseconds */
  if (u == 'u')
    factor = 1;               /* microseconds */
  else if (u == 'm')
    factor = 1000;            /* milliseconds */
  else if (u == 's')
    factor = 1000000;         /* seconds */
  if (_debug > 2)
    _out << "factor is " << factor << "\n";

  char* ptr = NULL;
  unsigned int i = strtoul(str, &ptr, 0);
  if (!ptr || *ptr != '.' || strlen(ptr) < 2 || factor == 1)
    return (i * factor);

  /* time specified in usecs can't have decimal points, so ignore them */
  if (factor == 1)
    return (i);

  unsigned int d = strtoul(ptr + 1, NULL, 0); /* integer and decimal, respectively */

  /* d is decimal, so get rid of excess digits */
  while (d >= factor)
    d /= 10;

  /* the last parenthesis avoids floating point exceptions. */
  return ((i * factor) + (d * (factor / 10)));
}

unsigned int check::_get_timevaldiff(timeval const* early, timeval const* later) {
  timeval now;
  if (!later) {
    gettimeofday(&now, NULL);
    later = &now;
  }
  if (!early)
    early = &_prog_start;

  /* if early > later we return 0 so as to indicate a timeout */
  if (early->tv_sec > later->tv_sec
      || (early->tv_sec == later->tv_sec
	  && early->tv_usec > later->tv_usec))
    return (0);

  unsigned int ret = (later->tv_sec - early->tv_sec) * 1000000;
  ret += later->tv_usec - early->tv_usec;

  return (ret);
}

int check::_recvfrom_wto(int sock,
			 void* buf,
			 unsigned int len,
			 sockaddr* saddr,
			 unsigned int* timo) {
  if (!*timo) {
    if (_debug)
      _out << "*timo is not\n";
    return (0);
  }

  timeval to;
  to.tv_sec = *timo / 1000000;
  to.tv_usec = (*timo - (to.tv_sec * 1000000));

  fd_set rd;
  FD_ZERO(&rd);
  FD_SET(sock, &rd);

  fd_set wr;
  FD_ZERO(&wr);

  errno = 0;

  timeval then;
  gettimeofday(&then, NULL);
  int n = select(sock + 1, &rd, &wr, NULL, &to);
  if (n < 0) {
    _out << QCoreApplication::applicationName() << ": select() in recvfrom_wto:" << strerror(errno) << "\n";
    return (n);
  }

  timeval now;
  gettimeofday(&now, NULL);
  *timo = _get_timevaldiff(&then, &now);

  if (!n)
    return (0); /* timeout */

  unsigned int slen = sizeof(sockaddr);
  return (recvfrom(sock, buf, len, 0, saddr, &slen));
}

/* response structure:
 * ip header   : 20 bytes
 * icmp header : 28 bytes
 * icmp echo reply : the rest
 */
int check::_wait_for_reply(int sock, unsigned int t) {
  /* if we can't listen or don't have anything to listen to, just return */
  if (!t || !(_icmp_sent - (_icmp_recv + _icmp_lost)))
    return (0);

  timeval wait_start;
  gettimeofday(&wait_start, NULL);

  unsigned int i = t;
  unsigned int per_pkt_wait = t / (_icmp_sent - (_icmp_recv + _icmp_lost));
  while ((_icmp_sent - (_icmp_recv + _icmp_lost))
	 && _get_timevaldiff(&wait_start, NULL) < i) {
    t = per_pkt_wait;

    /* wrap up if all _targets are declared dead */
    unsigned int timediff = _get_timevaldiff(&_prog_start, NULL);
    if (!(_targets - _targets_down)
	|| timediff >= _max_completion_time
	|| timediff >= _timeout
	|| (_mode == HOSTCHECK && _targets_down))
      return (1); // finish.

    /* reap responses until we hit a timeout */
    sockaddr_in resp_addr;
    int n = _recvfrom_wto(sock, _buf, sizeof(_buf), (sockaddr*)&resp_addr, &t);
    if (!n) {
      if (_debug > 1)
	_out << "recvfrom_wto() timed out during a " << per_pkt_wait << " usecs wait\n";
      continue;	/* timeout for this one, so keep trying */
    }
    if (n < 0) {
      if (_debug)
	_out << "recvfrom_wto() returned errors\n";
      return (n); // error
    }

    ip* ip = (struct ip*)_buf;
    if (_debug > 1)
      _out << "received " << ntohs(ip->ip_len)
	   << " bytes from " << inet_ntoa(resp_addr.sin_addr) << "\n";

    /* obsolete. alpha on tru64 provides the necessary defines, but isn't broken */
    /* #if defined( __alpha__ ) && __STDC__ && !defined( __GLIBC__ ) */
    /* alpha headers are decidedly broken. Using an ansi compiler,
     * they provide ip_vhl instead of ip_hl and ip_v, so we mask
     * off the bottom 4 bits */
    /* 		hlen = (ip->ip_vhl & 0x0f) << 2; */
    /* #else */
    int hlen = ip->ip_hl << 2;
    /* #endif */

    if (n < hlen + ICMP_MINLEN) {
      _out << QCoreApplication::applicationName() << ": received packet too short for ICMP ("
	   << n << " bytes, expected " << hlen + _icmp_pkt_size
	   << ") from " << inet_ntoa(resp_addr.sin_addr);
      if (errno)
	_out << ":" << strerror(errno) << "\n";
      return (-1); // error
    }
    // else if (_debug)
    //   _out << "ip header size: " << hlen
    // 	     << ", packet size: " << ntohs(ip->ip_len) - hlen
    // 	     << " (expected " << sizeof(ip) << ", " << _icmp_pkt_size << ")\n",

    /* check the response */
    ::icmp icp;
    memcpy(&icp, _buf + hlen, sizeof(icp));

    if (ntohs(icp.icmp_id) != _pid
	|| icp.icmp_type != ICMP_ECHOREPLY
	|| ntohs(icp.icmp_seq) >= _targets * _packets) {
      if (_debug > 2)
	_out << "not a proper ICMP_ECHOREPLY\n";
      _handle_random_icmp(_buf + hlen, &resp_addr);
      continue;
    }

    /* this is indeed a valid response */
    icmp_ping_data data;
    memcpy(&data, icp.icmp_data, sizeof(data));
    if (_debug > 2)
      _out << "ICMP echo-reply of len " << sizeof(data)
	   << ", id " << ntohs(icp.icmp_id)
	   << ", seq " << ntohs(icp.icmp_seq)
	   << ", cksum 0x" << hex << icp.icmp_cksum << dec << "\n";

    rta_host* host = _list_host[ntohs(icp.icmp_seq) / _packets];

    timeval now;
    gettimeofday(&now, NULL);

    unsigned int tdiff = _get_timevaldiff(&data.stime, &now);

    host->time_waited += tdiff;
    host->icmp_recv++;
    _icmp_recv++;
    if (tdiff > host->rtmax)
      host->rtmax = tdiff;
    if (tdiff < host->rtmin)
      host->rtmin = tdiff;

    if (_debug)
      _out << (float)tdiff / 1000 << " ms rtt from " << inet_ntoa(resp_addr.sin_addr)
	   << ", outgoing ttl: " << static_cast<unsigned int>(_ttl)
	   << ", incoming ttl: " << static_cast<unsigned int>(ip->ip_ttl)
	   << ", max: " << (float)host->rtmax / 1000
	   << ", min: " << (float)host->rtmin / 1000 << "\n";

    /* if we're in hostcheck mode, exit with limited printouts */
    if (_mode == HOSTCHECK) {
      _out << "OK - " << host->name << " responds to ICMP. Packet " << _icmp_recv
	   << ", rta " << (float)tdiff / 1000 << "ms|pkt=" << _icmp_recv << ";;0;"
	   << _packets << " rta=" << (float)tdiff / 1000 << ";"
	   << (float)_warn.rta / 1000 << ";" << (float)_crit.rta / 1000 << ";;\n";
      return (1); // finish.
    }
  }

  return (0); // continue.
}

unsigned short check::_icmp_checksum(unsigned short* p, int n) {
  long sum = 0;
  while (n > 1) {
    sum += *p++;
    n -= 2;
  }

  /* mop up the occasional odd byte */
  if (n == 1)
    sum += (unsigned char)*p;

  sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
  sum += (sum >> 16);			/* add carry */
  unsigned short cksum = ~sum;          /* ones-complement, trunc to 16 bits */

  return (cksum);
}

/* the ping functions */
int check::_send_icmp_ping(int sock, rta_host* host) {
  union {
    char*           buf;
    ::icmp*         icp;
    unsigned short* cksum_in;
  } packet = { NULL };

  sockaddr* addr = (sockaddr*)&host->saddr_in;

  packet.buf = new char[_icmp_pkt_size];
  memset(packet.buf, 0, _icmp_pkt_size);

  timeval tv;
  if (gettimeofday(&tv, NULL) == -1)
    return (-1);

  icmp_ping_data data;
  data.ping_id = 10; /* host->icmp.icmp_sent; */
  memcpy(&data.stime, &tv, sizeof(tv));
  memcpy(&packet.icp->icmp_data, &data, sizeof(data));
  packet.icp->icmp_type = ICMP_ECHO;
  packet.icp->icmp_code = 0;
  packet.icp->icmp_cksum = 0;
  packet.icp->icmp_id = htons(_pid);
  packet.icp->icmp_seq = htons(host->id++);
  packet.icp->icmp_cksum = _icmp_checksum(packet.cksum_in, _icmp_pkt_size);

  if (_debug > 2)
    _out << "Sending ICMP echo-request of len " << sizeof(data)
	 << ", id " << ntohs(packet.icp->icmp_id)
	 << ", seq " << ntohs(packet.icp->icmp_seq)
	 << ", cksum 0x" << hex << packet.icp->icmp_cksum << dec
	 << " to host " << host->name << "\n";

  long int len = sendto(sock,
			packet.buf,
			_icmp_pkt_size,
			0,
			(sockaddr*)addr,
			sizeof(sockaddr));
  delete[] packet.buf;
  if (len < 0 || (unsigned int)len != _icmp_pkt_size) {
    if (_debug)
      _out << "Failed to send ping to " << inet_ntoa(host->saddr_in.sin_addr) << "\n";
    return (-1);
  }

  _icmp_sent++;
  host->icmp_sent++;

  return (0);
}

int check::_run_checks(void) {
  /* this loop might actually violate the pkt_interval or target_interval
   * settings, but only if there aren't any _packets on the wire which
   * indicates that the target can handle an increased packet rate */
  for (unsigned int i = 0; i < _packets; i++) {
    for (unsigned int t = 0; t < _targets; t++) {
      /* don't send useless _packets */
      if (!(_targets - _targets_down))
	return (1);
      if (_list_host[t]->flags & FLAG_LOST_CAUSE) {
	if (_debug)
	  _out << _list_host[t]->name << " is a lost cause. not sending any more\n";
	continue;
      }

      /* we're still in the game, so send next packet */
      _send_icmp_ping(_icmp_sock, _list_host[t]);
      int ret = _wait_for_reply(_icmp_sock, _target_interval);
      if (ret)
	return (ret);
    }
    int ret = _wait_for_reply(_icmp_sock, _pkt_interval * _targets);
    if (ret)
      return (ret);
  }

  if ((_icmp_sent - (_icmp_recv + _icmp_lost)) && (_targets - _targets_down)) {
    unsigned int time_passed = _get_timevaldiff(NULL, NULL);
    unsigned int final_wait = _max_completion_time - time_passed;

    if (_debug)
      _out << "time_passed: " << time_passed
	   << "  final_wait: " << final_wait
	   << "  max_completion_time: " << _max_completion_time << "\n";
    if (time_passed > _max_completion_time) {
      if (_debug)
	_out << "Time passed. Finishing up\n";
      return (1);
    }

    /* catch the _packets that might come in within the timeframe, but
     * haven't yet */
    if (_debug)
      _out << "Waiting for " << final_wait << " micro-seconds ("
	   << (float)final_wait / 1000 << " msecs)\n";
    int ret = _wait_for_reply(_icmp_sock, final_wait);
    if (ret)
      return (ret);
  }
  return (1);
}

int check::_add_target_ip(char const* arg, in_addr const* in) {
  /* disregard obviously stupid addresses */
  if (in->s_addr == INADDR_NONE || in->s_addr == INADDR_ANY)
    return (-1);

  /* no point in adding two identical IP's, so don't. ;) */
  for (QList<rta_host*>::const_iterator it = _list_host.begin(), end = _list_host.end();
       it != end;
       ++it) {
    if ((*it)->saddr_in.sin_addr.s_addr == in->s_addr) {
      if (_debug)
	_out << "Identical IP already exists. Not adding " << arg << "\n";
      return (-1);
    }
  }

  /* add the fresh ip */
  rta_host* host = new rta_host();
  memset(host, 0, sizeof(rta_host));

  /* set the values. use calling name for _out */
  host->name = new char[strlen(arg) + 1];
  strcpy(host->name, arg);

  /* fill out the sockaddr_in */
  host->saddr_in.sin_family = AF_INET;
  host->saddr_in.sin_addr.s_addr = in->s_addr;

  host->rtmin = DBL_MAX;

  _list_host.push_back(host);
  _targets++;

  return (0);
}

/* wrapper for add_target_ip */
int check::_add_target(char const* arg) {
  in_addr ip;
  /* don't resolve if we don't have to */
  if ((ip.s_addr = inet_addr(arg)) != INADDR_NONE) {
    _add_target_ip(arg, &ip);
    return (0);
  }

  hostent* he = gethostbyname(arg);
  if (!he) {
    _out << QCoreApplication::applicationName() << ": Failed to resolve " << arg << "\n";
    return (-1);
  }

  /* possibly add all the IP's as _targets */
  for (int i = 0; he->h_addr_list[i]; i++) {
    in_addr* in = reinterpret_cast<in_addr*>(he->h_addr_list[i]);
    _add_target_ip(arg, in);

    /* this is silly, but it works */
    if (_mode == HOSTCHECK || _mode == ALL) {
      if (_debug > 2)
	_out << "mode: " << _mode << "\n";
      continue;
    }
    break;
  }

  return (0);
}

/* TODO: Move this to netutils.c and also change check_dhcp to use that. */
in_addr_t check::_get_ip_address(char const* ifname) {
#if defined(SIOCGIFADDR)
  ifreq ifr;
  strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';
  if (ioctl(_icmp_sock, SIOCGIFADDR, &ifr) == -1) {
    _out << QCoreApplication::applicationName()
	 << ": Cannot determine IP address of interface " << ifname
	 << ": " << strerror(errno) << "\n";
    return (0);
  }

  sockaddr_in ip;
  memcpy(&ip, &ifr.ifr_addr, sizeof(ip));
  return (ip.sin_addr.s_addr);
#else
  (void)ifname;
  _out << QCoreApplication::applicationName()
       << ": Cannot get interface IP address on this platform.\n";
  return (0);
#endif
}

bool check::_set_source_ip(char const* arg) {
  sockaddr_in src;
  memset(&src, 0, sizeof(src));
  src.sin_family = AF_INET;
  if ((src.sin_addr.s_addr = inet_addr(arg)) == INADDR_NONE)
    if ((src.sin_addr.s_addr = _get_ip_address(arg)) == 0)
      return (false);

  if (bind(_icmp_sock, (sockaddr*)&src, sizeof(src)) == -1) {
    _out << QCoreApplication::applicationName() << ": Cannot bind to IP address " << arg
	 << ": " << strerror(errno) << "\n";
    return (false);
  }
  return (true);
}

/* not too good at checking errors, but it'll do (main() should barfe on -1) */
int check::_get_threshold(char const* str, threshold* th) {
  int size = strlen(str);
  if (!str || !size || !th)
    return (-1);

  char* dup = new char[size + 1];
  strcpy(dup, str);

  /* pointer magic slims code by 10 lines. i is bof-stop on stupid libc's */
  char i = 0;
  char* p = &dup[size - 1];
  while (p != &dup[1]) {
    if (*p == '%')
      *p = '\0';
    else if (*p == ',' && i) {
      *p = '\0';	/* reset it so get_timevar(str) works nicely later */
      th->pl = (unsigned char)strtoul(p + 1, NULL, 0);
      break;
    }
    i = 1;
    p--;
  }
  th->rta = _get_timevar(dup);

  delete[] dup;

  if (!th->rta)
    return (-1);

  if (th->rta > MAXTTL * 1000000)
    th->rta = MAXTTL * 1000000;
  if (th->pl > 100)
    th->pl = 100;

  return (0);
}

int check::_finish(void) {
  static char const* status_string[] = { "OK", "WARNING", "CRITICAL", "UNKNOWN", "DEPENDENT" };
  int status = OK;

  if (_debug)
    _out << "icmp_sent: " << _icmp_sent
	 << "  icmp_recv: " << _icmp_recv
	 << "  icmp_lost: " << _icmp_lost << "\n"
	 << "_targets: " << _targets
	 << "  targets_alive: " << (_targets - _targets_down) << "\n";

  /* iterate thrice to calculate values, give _out, and print perfparse */
  int hosts_ok = 0;
  int hosts_warn = 0;
  for (QList<rta_host*>::iterator it = _list_host.begin(), end = _list_host.end(); it != end; ++it) {
    unsigned char pl;
    double rta;
    if (!(*it)->icmp_recv) {
      /* rta 0 is ofcourse not entirely correct, but will still show up
       * conspicuosly as missing entries in perfparse and cacti */
      pl = 100;
      rta = 0;
      status = CRITICAL;
      /* up the down counter if not already counted */
      if (!((*it)->flags & FLAG_LOST_CAUSE) && (_targets - _targets_down))
	_targets_down++;
    }
    else {
      pl = (((*it)->icmp_sent - (*it)->icmp_recv) * 100) / (*it)->icmp_sent;
      rta = (double)(*it)->time_waited / (*it)->icmp_recv;
    }
    (*it)->pl = pl;
    (*it)->rta = rta;
    if (pl >= _crit.pl || rta >= _crit.rta)
      status = CRITICAL;
    else if (!status && (pl >= _warn.pl || rta >= _warn.rta)) {
      status = WARNING;
      hosts_warn++;
    }
    else
      hosts_ok++;
  }

  /* this is inevitable */
  if (!(_targets - _targets_down))
    status = CRITICAL;
  if (_min_hosts_alive > -1) {
    if (hosts_ok >= _min_hosts_alive)
      status = OK;
    else if ((hosts_ok + hosts_warn) >= _min_hosts_alive)
      status = WARNING;
  }
  _out << status_string[status] << " - ";

  unsigned int i = 0;
  for (QList<rta_host*>::iterator it = _list_host.begin(), end = _list_host.end(); it != end; ++it) {
    if (_debug)
      _out << "\n";
    if (i) {
      if (i < _targets)
	_out << " :: ";
      else
	_out << "\n";
    }
    i++;
    if (!(*it)->icmp_recv) {
      status = CRITICAL;
      if ((*it)->flags & FLAG_LOST_CAUSE)
	_out << (*it)->name << ": " << _get_icmp_error_msg((*it)->icmp_type, (*it)->icmp_code)
	     << " @ " << inet_ntoa((*it)->error_addr) << ". rta nan, lost 100%";
      else /* not marked as lost cause, so we have no flags for it */
	_out << (*it)->name << ": rta nan, lost 100%";
    }
    else	/* !icmp_recv */
      _out << (*it)->name << ": rta " << (*it)->rta / 1000 << "ms, lost " << (*it)->pl << "%";
  }

  /* iterate once more for pretty perfparse _out */
  _out << "|";
  i = 0;
  for (QList<rta_host*>::iterator it = _list_host.begin(), end = _list_host.end(); it != end; ++it) {
    if (_debug)
      _out << "\n";
    _out << (_targets > 1 ? (*it)->name : "") << "rta=" << (*it)->rta / 1000 << "ms;"
	 << (float)_warn.rta / 1000 << ";" << (float)_crit.rta / 1000 << ";0; "
	 << (_targets > 1 ? (*it)->name : "") << "pl=" << (*it)->pl
	 << "%;" << _warn.pl << ";" << _crit.pl << ";; "
	 << (_targets > 1 ? (*it)->name : "") << "rtmax=" << (float)(*it)->rtmax / 1000 << "ms;;;; "
	 << (_targets > 1 ? (*it)->name : "") << "rtmin="
	 << ((*it)->rtmin < DBL_MAX ? (float)(*it)->rtmin / 1000 : (float)0) << "ms;;;; ";

    delete[] (*it)->name;
    delete *it;
  }
  _list_host.clear();

  if (_min_hosts_alive > -1) {
    if (hosts_ok >= _min_hosts_alive)
      status = OK;
    else if ((hosts_ok + hosts_warn) >= _min_hosts_alive)
      status = WARNING;
  }

  /* finish with an empty line */
  _out << "\n";
  if (_debug)
    _out << "_targets: " << _targets
	 << ", targets_alive: " << (_targets - _targets_down)
	 << ", hosts_ok: " << hosts_ok
	 << ", hosts_warn: " << hosts_warn
	 << ", min_hosts_alive: " << _min_hosts_alive << "\n";

  return (status);
}
