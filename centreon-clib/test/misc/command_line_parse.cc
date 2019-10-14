/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <iostream>
#include <vector>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/command_line.hh"

using namespace com::centreon::misc;

/**
 *  Check command line parsing.
 *
 *  @param[in] cmdline  The command line to parse.
 *  @param[in] res      List of result to compare.
 *
 *  @return True on success, otherwise false.
 */
static bool check(std::string const& cmdline,
                  std::vector<std::string> const& res) {
  try {
    command_line cmd;
    cmd.parse(cmdline);
    if (cmd.get_argc() != static_cast<int>(res.size()))
      return (false);
    char** argv(cmd.get_argv());
    for (int i(0), end(cmd.get_argc()); i < end; ++i)
      if (argv[i] != res[i])
        return (false);
    if (argv[cmd.get_argc()])
      return (false);
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

/**
 *  Check with invalid command line.
 *
 *  @return True on success, otherwise false.
 */
static bool check_invalid_cmdline() {
  try {
    command_line cmd;
    cmd.parse("'12 12");
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the set description.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    if (!check_invalid_cmdline())
      throw(
          basic_error() << "parsing error: try to parse invalid command line");

    {
      std::string cmdline("\\ echo -n \"test\"");
      std::vector<std::string> res;
      res.push_back(" echo");
      res.push_back("-n");
      res.push_back("test");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("test \"|\" \"\" \"|\"");
      std::vector<std::string> res;
      res.push_back("test");
      res.push_back("|");
      res.push_back("");
      res.push_back("|");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("");
      std::vector<std::string> res;
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("        \t\t\t\t\t       ");
      std::vector<std::string> res;
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("aa\tbbb c\tdd ee");
      std::vector<std::string> res;
      res.push_back("aa");
      res.push_back("bbb");
      res.push_back("c");
      res.push_back("dd");
      res.push_back("ee");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline(" aa    bbb \t  c  dd  ee    \t");
      std::vector<std::string> res;
      res.push_back("aa");
      res.push_back("bbb");
      res.push_back("c");
      res.push_back("dd");
      res.push_back("ee");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("    'aa bbb   cc' dddd   ");
      std::vector<std::string> res;
      res.push_back("aa bbb   cc");
      res.push_back("dddd");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("    \" aa bbb   cc \" dddd");
      std::vector<std::string> res;
      res.push_back(" aa bbb   cc ");
      res.push_back("dddd");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("  ' \" aa bbb bbb  cc ' ");
      std::vector<std::string> res;
      res.push_back(" \" aa bbb bbb  cc ");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline(" \" '\\n aaa 42 ' \" 4242 ");
      std::vector<std::string> res;
      res.push_back(" '\n aaa 42 ' ");
      res.push_back("4242");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("\\\\\" 1 2 \"");
      std::vector<std::string> res;
      res.push_back("\\ 1 2 ");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("\\\\\\\" 1 2 \\\"");
      std::vector<std::string> res;
      res.push_back("\\\"");
      res.push_back("1");
      res.push_back("2");
      res.push_back("\"");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline(" '12\\t ''34 56' \t \" 12 12 12 \" '99 9 9'");
      std::vector<std::string> res;
      res.push_back("12\t 34 56");
      res.push_back(" 12 12 12 ");
      res.push_back("99 9 9");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }

    {
      std::string cmdline("\\.\\/\\-\\*\\1");
      std::vector<std::string> res;
      res.push_back("./-*1");
      if (!check(cmdline, res))
        throw(basic_error() << "parsing error: " << cmdline);
    }
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  return (ret);
}
