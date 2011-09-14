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

#ifndef CCC_ICMP_CHECK_HH_
# define CCC_ICMP_CHECK_HH_

# include <QRunnable>
# include <QTextStream>
# include <QList>
# include <QString>
# include <QStringList>
# include <sys/types.h>
# include <arpa/inet.h>
# include <netinet/ip_icmp.h>
# include "com/centreon/connector/icmp/socket_manager.hh"

namespace                      com {
  namespace                    centreon {
    namespace                  connector {
      namespace                icmp {
        /**
         *  @class check check.hh
         *  @brief Check if a host is alive.
         *
         *  Check if a specifique host is alive.
         */
        class                  check : public QObject,
                                       public QRunnable {
          Q_OBJECT

         public:
                               check(unsigned long cmd_id,
                                 QStringList const& arguments,
                                 unsigned int timeout);
                               ~check() throw();

          unsigned long        get_command_id() const throw();
          QString const&       get_output();
          int                  get_exit_code() const throw();

         signals:
          void                 start();
          void                 finish(unsigned long cmd_id,
                                      QString const& output,
                                      int exit_code);

         protected:
          void                 run();

         private:
          enum e_state {
            OK = 0,
            WARNING,
            CRITICAL,
            UNKNOWN,
            DEPENDENT
          };

          enum e_mode {
            RTA = 0,       /* Send all packets no matter what (mimic check_icmp and check_ping). */
            HOSTCHECK = 1, /*
                            * Return immediately upon any sign of life
                            * In addition, sends packets to ALL addresses assigned
                            * to this host (as returned by gethostbyname() or
                            * gethostbyaddr() and expects one host only to be checked at
                            * a time.  Therefore, any packet response what so ever will
                            * count as a sign of life, even when received outside
                            * crit.rta limit. Do not misspell any additional IP's.
                            */
            ALL = 2,        /* Requires packets from ALL requested IP to return OK (default). */
            ICMP = 3        /*
                             * Implement something similar to check_icmp (MODE_RTA without
                             * tcp and udp args does this)
                             */
          };

          struct               rta_host {
            unsigned short     id;          /* id in **table, and icmp pkts */
            char*              name;        /* arg used for adding this host */
            char*              msg;         /* icmp error message, if any */
            sockaddr_in        saddr_in;    /* the address of this host */
            in_addr            error_addr;  /* stores address of error replies */
            unsigned long long time_waited; /* total time waited, in usecs */
            unsigned int       icmp_sent;
            unsigned int       icmp_recv;
            unsigned int       icmp_lost;   /* counters */
            unsigned char      icmp_type;
            unsigned char      icmp_code;   /* type and code from errors */
            unsigned short     flags;       /* control/status flags */
            double             rta;         /* measured RTA */
            double             rtmax;       /* max rtt */
            double             rtmin;       /* min rtt */
            unsigned char      pl;          /* measured packet loss */
          };

          // threshold structure. all values are maximum allowed, exclusive
          struct               threshold {
            unsigned char      pl;  // max allowed packet loss in percent
            unsigned int       rta; // roundtrip time average, microseconds
          };

          // the data structure
          struct               icmp_ping_data {
            timeval            stime;   // timestamp (saved in protocol struct as well)
            unsigned short     ping_id;
          };

          bool                 _parse_args();

          char const*          _get_icmp_error_msg(unsigned char icmp_type,
                                 unsigned char icmp_code);
          int                  _handle_random_icmp(unsigned char const* packet,
                                 sockaddr_in const* addr);
          void                 _print_help(void);
          unsigned int         _get_timevar(char const* str);
          unsigned int         _get_timevaldiff(timeval const* early, timeval const* later);
          int                  _recvfrom_wto(int sock,
                                 void* buf,
                                 unsigned int len,
                                 sockaddr* saddr,
                                 unsigned int* timo);
          int                  _wait_for_reply(int sock,
                                 unsigned int t);
          unsigned short       _icmp_checksum(unsigned short* p, int n);
          int                  _send_icmp_ping(int sock, rta_host* host);
          int                  _run_checks(void);
          int                  _add_target_ip(char const* arg, in_addr const* in);
          int                  _add_target(char const* arg);
          in_addr_t            _get_ip_address(char const* ifname);
          bool                 _set_source_ip(char const* arg);
          int                  _get_threshold(char const* str, threshold* th);
          int                  _finish(void);

          QString              _out_buffer;
          QTextStream          _out;
          QList<rta_host*>     _list_host;
          QStringList          _arguments;
          socket_manager&      _sock_manager;
          threshold            _crit;
          threshold            _warn;
          unsigned long long   _max_completion_time;
          unsigned long        _cmd_id;
          unsigned int         _debug;
          unsigned int         _icmp_sent;
          unsigned int         _icmp_recv;
          unsigned int         _icmp_lost;
          unsigned int         _pkt_interval;
          unsigned int         _timeout;
          int                  _min_hosts_alive;
          int                  _mode;
          int                  _icmp_sock;
          int                  _target_interval;
          int                  _exit_code;
          pid_t                _pid;
          unsigned short       _targets_down;
          unsigned short       _targets;
          unsigned short       _packets;
          unsigned short       _icmp_data_size;
          unsigned short       _icmp_pkt_size;
          unsigned char        _ttl;
          unsigned char        _buf[4096];
          bool                 _is_running;

          timeval              _prog_start;

          static const float   _pkt_backoff_factor;
          static const float   _target_backoff_factor;

          static const unsigned int FLAG_LOST_CAUSE = 0x01;  // decidedly dead target.

          static const unsigned int MIN_PING_DATA_SIZE = sizeof(icmp_ping_data);
          static const unsigned int MAX_IP_PKT_SIZE = 65536;
          static const unsigned int IP_HDR_SIZE = 20;
          static const unsigned int MAX_PING_DATA = MAX_IP_PKT_SIZE - IP_HDR_SIZE - ICMP_MINLEN;
          static const unsigned int DEFAULT_PING_DATA_SIZE = MIN_PING_DATA_SIZE + 44;
        };
      }
    }
  }
}

#endif // !CCC_ICMP_CHECK_HH_
