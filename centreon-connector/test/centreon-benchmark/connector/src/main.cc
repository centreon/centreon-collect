/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <getopt.h>
#include <libgen.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/connector.hh"
#include "com/centreon/benchmark/connector/plugin.hh"

using namespace com::centreon::benchmark::connector;

struct options {
  options()
    : limit_running(1024),
      memory_usage(0),
      is_plugin(false),
      total_request(1) {}
  std::list<std::string>   args;
  std::string              commands_file;
  unsigned int             limit_running;
  unsigned int             memory_usage;
  std::string              output_file;
  bool                     is_plugin;
  unsigned int             total_request;
};

static void usage(char* appname) {
  std::cout
    << "usage: " << basename(appname)
    << " -c commands_file [-t connector|plugin] [-m 1024] [-n 100] [-l 1024] args..."
    << std::endl;
}

static void help() {
  std::cout
    << "  -c, --commands-file:      Path of commands file\n"
    << "  -h, --help:               This help\n"
    << "  -l, --limit-concurrency:  Max concurrency request (1024)\n"
    << "  -m, --memory-usage:       Size of prealocate memory (0 Mo)\n"
    << "  -n, --total-request:      Number of total request\n"
    << "  -o, --output:             The file path to write request output\n"
    << "  -t, --type:               Type of running command (connector or plugin)"
    << std::endl;
}

static options parse_options(int ac, char** av) {
  static struct option loptions[] = {
    { "commands-file",     1, NULL, 'c' },
    { "help",              0, NULL, 'h' },
    { "limit-concurrency", 1, NULL, 'l' },
    { "memory-usage",      1, NULL, 'm' },
    { "total-request",     1, NULL, 'n' },
    { "output",            1, NULL, 'o' },
    { "type",              1, NULL, 't' },
    { NULL,                0, NULL, 0}
  };

  options opt;
  char* appname(av[0]);
  int ret;
  while ((ret = getopt_long(ac, av, "c:hl:m:n:o:t:", loptions, NULL)) != -1) {
    switch (ret) {
    case 'c':
      opt.commands_file = optarg;
      break;

    case 'l':
      opt.limit_running = atoi(optarg);
      break;

    case 'm':
      opt.memory_usage = atoi(optarg);
      break;

    case 'n':
      opt.total_request = atoi(optarg);
      break;

    case 'o':
      opt.output_file = optarg;
      break;

    case 't':
      opt.is_plugin = !strcmp(optarg, "plugin");
      break;

    case 'h':
    default:
      usage(appname);
      help();
      exit(EXIT_SUCCESS);
    }
  }

  while (optind < ac)
    opt.args.push_back(av[optind++]);

  if (!opt.args.size() && !opt.is_plugin)
    throw (basic_exception("invalid args"));
  if (opt.commands_file.empty())
    throw (basic_exception("invalid commands file"));
  if (!opt.limit_running)
    throw (basic_exception("invalid limit concurrency"));
  if (!opt.total_request)
    throw (basic_exception("invalid total request"));

  return (opt);
}

int main(int argc, char** argv) {
  int ret(EXIT_SUCCESS);
  try {
    if (argc == 1) {
      usage(argv[0]);
      help();
      exit(EXIT_SUCCESS);
    }
    std::auto_ptr<benchmark> bench(NULL);
    options opt(parse_options(argc, argv));

    if (opt.is_plugin)
      bench = std::auto_ptr<benchmark>(
                     new plugin(opt.commands_file, opt.args));
    else
      bench = std::auto_ptr<benchmark>(
                     new connector(opt.commands_file, opt.args));

    bench->set_limit_running(opt.limit_running);
    bench->set_memory_usage(opt.memory_usage);
    bench->set_total_request(opt.total_request);
    bench->set_output_file(opt.output_file);

    bench->run();
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = EXIT_FAILURE;
  }
  return (ret);
}
