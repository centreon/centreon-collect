/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include <gtest/gtest.h>

#include <limits.h>
#include <math.h>
#include <iostream>
#include <string>
#include "com/centreon/misc/argument.hh"
#include "com/centreon/misc/command_line.hh"
#include "com/centreon/misc/get_options.hh"
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

static bool check_argument(argument const& a1, argument const& a2) {
  if (a1.get_long_name() != a2.get_long_name())
    return false;
  if (a1.get_description() != a2.get_description())
    return false;
  if (a1.get_name() != a2.get_name())
    return false;
  if (a1.get_has_value() != a2.get_has_value())
    return false;
  if (a1.get_is_set() != a2.get_is_set())
    return false;
  if (a1.get_value() != a2.get_value())
    return false;
  return true;
}

static bool compare(command_line const& cmd1, command_line const& cmd2) {
  if (cmd1.get_argc() != cmd2.get_argc())
    return (false);
  char* const* argv1(cmd1.get_argv());
  char* const* argv2(cmd2.get_argv());
  for (int i(0), end(cmd1.get_argc()); i < end; ++i)
    if (strcmp(argv1[i], argv2[i]))
      return (false);
  if (argv1[cmd1.get_argc()] != argv2[cmd2.get_argc()])
    return (false);
  return (true);
}

static bool check(std::string const& cmdline,
                  std::vector<std::string> const& res) {
  try {
    command_line cmd;
    cmd.parse(cmdline);
    if (cmd.get_argc() != static_cast<int>(res.size()))
      return (false);
    char* const* argv(cmd.get_argv());
    for (int i(0), end(cmd.get_argc()); i < end; ++i)
      if (argv[i] != res[i])
        return (false);
    if (argv[cmd.get_argc()])
      return (false);
  } catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

static bool check_invalid_cmdline() {
  try {
    command_line cmd;
    cmd.parse("'12 12");
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

class my_options : public get_options {
 public:
  my_options(std::vector<std::string> const& args) : get_options() {
    _arguments['a'] = argument("arg", 'a', "", true);
    _arguments['c'] = argument("cold", 'c', "", true);
    _arguments['t'] = argument("test", 't', "", true);
    _arguments['h'] = argument("help", 'h');
    _arguments['d'] = argument("default", 'd', "", true, true, "def");
    _parse_arguments(args);
  }
  my_options(my_options const& right) : get_options(right) {}
  ~my_options() throw() {}
  my_options& operator=(my_options const& right) {
    get_options::operator=(right);
    return (*this);
  }
};

static bool check_unknown_option() {
  try {
    std::vector<std::string> args;
    args.push_back("--test=1");
    args.push_back("-h");
    args.push_back("--arg");
    args.push_back("2");
    args.push_back("--failed=42");
    args.push_back("param1");
    args.push_back("param2");
    args.push_back("param3");

    my_options opt(args);
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool check_require_argument() {
  try {
    std::vector<std::string> args;
    args.push_back("-h");
    args.push_back("--arg");
    args.push_back("2");
    args.push_back("--test");

    my_options opt(args);
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool invalid_long_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    opt.get_argument("unknown");
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool valid_long_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    argument& a1(opt.get_argument("help"));
    argument const& a2(opt.get_argument("help"));
    (void)a1;
    (void)a2;
  } catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

static bool invalid_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    opt.get_argument('*');
  } catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

static bool valid_name() {
  try {
    std::vector<std::string> args;
    my_options opt(args);
    argument& a1(opt.get_argument('h'));
    argument const& a2(opt.get_argument('h'));
    (void)a1;
    (void)a2;
  } catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

static bool check_double(double d) {
  stringifier buffer;
  buffer << d;
  char* ptr(NULL);
  double converted(strtod(buffer.data(), &ptr));
  return (ptr && !*ptr &&
          (fabs(d - converted)
           // Roughly 0.1% error margin.
           <= (fabs(d / 1000) + 2 * DBL_EPSILON)));
}

/**
 *  Check the argument constructor.
 *
 *  @return 0 on success.
 */
TEST(ClibMisc, ArgumentCtor) {
  std::string long_name("help");
  char name('h');
  std::string description("this help");
  bool has_value(true);
  bool is_set(true);
  std::string value("help:\n --help, -h  this help");
  argument arg(long_name, name, description, has_value, is_set, value);

  ASSERT_EQ(arg.get_long_name(), long_name);

  ASSERT_EQ(arg.get_name(), name);
  ASSERT_EQ(arg.get_description(), description);
  ASSERT_EQ(arg.get_has_value(), has_value);
  ASSERT_EQ(arg.get_is_set(), is_set);
  ASSERT_EQ(arg.get_value(), value);
}

TEST(ClibMisc, ArgumentCopy) {
  argument ref("help", 'c', "this help", true, true,
               "help:\n --help, -h  this help");

  argument arg1(ref);
  ASSERT_TRUE(check_argument(ref, arg1));

  argument arg2 = ref;
  ASSERT_TRUE(check_argument(ref, arg1));
}

TEST(ClibMisc, ArgumentEqual) {
  argument ref("help", 'c', "this help", true, true,
               "help:\n --help, -h  this help");

  argument arg1(ref);
  ASSERT_TRUE(ref == arg1);

  argument arg2 = ref;
  ASSERT_TRUE((ref == arg2));
}

TEST(ClibMisc, ArgumentNotEqual) {
  argument ref("help", 'c', "this help", true, true,
               "help:\n --help, -h  this help");

  argument arg1(ref);
  ASSERT_EQ(ref, arg1);

  argument arg2 = ref;
  ASSERT_EQ(ref, arg2);
}

TEST(ClibMisc, ArgumentSetDescription) {
  std::string description("this help");
  argument arg;
  arg.set_description(description);
  ASSERT_EQ(arg.get_description(), description);
}

TEST(ClibMisc, ArgumentSetHasValue) {
  bool has_value(true);
  argument arg;
  arg.set_has_value(has_value);
  ASSERT_EQ(arg.get_has_value(), has_value);
}

TEST(ClibMisc, ArgumentSetIsSet) {
  bool is_set(true);
  argument arg;
  arg.set_is_set(is_set);
  ASSERT_EQ(arg.get_is_set(), is_set);
}

TEST(ClibMisc, ArgumentSetLongName) {
  std::string long_name("help");
  argument arg;
  arg.set_long_name(long_name);
  ASSERT_EQ(arg.get_long_name(), long_name);
}

TEST(ClibMisc, ArgumentSetName) {
  char name('h');
  argument arg;
  arg.set_name(name);
  ASSERT_EQ(arg.get_name(), name);
}

TEST(ClibMisc, ArgumentSetValue) {
  std::string value("help:\n --help, -h  this help.");
  argument arg;
  arg.set_value(value);
  ASSERT_EQ(arg.get_value(), value);
}

TEST(ClibMisc, CommandLineCtor) {
  command_line cmd;
  ASSERT_FALSE(cmd.get_argc());
  ASSERT_FALSE(cmd.get_argv());
}

TEST(ClibMisc, CommandLineCopy) {
  std::string cmdline("1 2 3 4 5");
  command_line ref(cmdline);

  command_line cmd1(ref);
  ASSERT_TRUE(compare(ref, cmd1));

  command_line cmd2 = ref;
  ASSERT_TRUE(compare(ref, cmd2));
}

TEST(ClibMisc, CommandLineEqual) {
  std::string cmdline(" 1 2 3 4 5 6 7 8 9 0 ");
  command_line cmd1(cmdline);
  command_line cmd2(cmdline);
  ASSERT_TRUE((cmd1 == cmd2));
}

TEST(ClibMisc, CommandLineGetArgc) {
  std::string cmdline(" 1 2 3 4 5 6 7 8 9 0 ");
  command_line cmd;
  ASSERT_FALSE(cmd.get_argc());
  cmd.parse(cmdline);
  ASSERT_EQ(cmd.get_argc(), 10);
}

TEST(ClibMisc, CommandLineGetArgv) {
  std::string cmdline("123456");
  command_line cmd;
  ASSERT_FALSE(cmd.get_argv());
  cmd.parse(cmdline);
  ASSERT_TRUE(cmd.get_argv());
  ASSERT_EQ(cmd.get_argv()[0], cmdline);
}

TEST(ClibMisc, CommandLineNotEqual) {
  std::string cmdline(" 1 2 3 4 5 6 7 8 9 0 ");
  command_line cmd1(cmdline);
  command_line cmd2(cmdline);
  ASSERT_EQ(cmd1, cmd2);
}

TEST(ClibMisc, CommandLineParse) {
  ASSERT_TRUE(check_invalid_cmdline());
  {
    std::string cmdline("\\ echo -n \"test\"");
    std::vector<std::string> res;
    res.push_back(" echo");
    res.push_back("-n");
    res.push_back("test");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("test \"|\" \"\" \"|\"");
    std::vector<std::string> res;
    res.push_back("test");
    res.push_back("|");
    res.push_back("");
    res.push_back("|");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("");
    std::vector<std::string> res;
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("        \t\t\t\t\t       ");
    std::vector<std::string> res;
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("aa\tbbb c\tdd ee");
    std::vector<std::string> res;
    res.push_back("aa");
    res.push_back("bbb");
    res.push_back("c");
    res.push_back("dd");
    res.push_back("ee");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" aa    bbb \t  c  dd  ee    \t");
    std::vector<std::string> res;
    res.push_back("aa");
    res.push_back("bbb");
    res.push_back("c");
    res.push_back("dd");
    res.push_back("ee");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("    'aa bbb   cc' dddd   ");
    std::vector<std::string> res;
    res.push_back("aa bbb   cc");
    res.push_back("dddd");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("    \" aa bbb   cc \" dddd");
    std::vector<std::string> res;
    res.push_back(" aa bbb   cc ");
    res.push_back("dddd");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("  ' \" aa bbb bbb  cc ' ");
    std::vector<std::string> res;
    res.push_back(" \" aa bbb bbb  cc ");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" \" '\\n aaa 42 ' \" 4242 ");
    std::vector<std::string> res;
    res.push_back(" '\n aaa 42 ' ");
    res.push_back("4242");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\\\\" 1 2 \"");
    std::vector<std::string> res;
    res.push_back("\\ 1 2 ");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\\\\\\" 1 2 \\\"");
    std::vector<std::string> res;
    res.push_back("\\\"");
    res.push_back("1");
    res.push_back("2");
    res.push_back("\"");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline(" '12\\t ''34 56' \t \" 12 12 12 \" '99 9 9'");
    std::vector<std::string> res;
    res.push_back("12\t 34 56");
    res.push_back(" 12 12 12 ");
    res.push_back("99 9 9");
    ASSERT_TRUE(check(cmdline, res));
  }

  {
    std::string cmdline("\\.\\/\\-\\*\\1");
    std::vector<std::string> res;
    res.push_back("./-*1");
    ASSERT_TRUE(check(cmdline, res));
  }
}

TEST(ClibMisc, GetOptionsCtor) {
  std::vector<std::string> args;
  args.push_back("-c1");
  args.push_back("--test=1");
  args.push_back("-h");
  args.push_back("--arg");
  args.push_back("2");
  args.push_back("param1");
  args.push_back("param2");
  args.push_back("param3");

  my_options opt(args);
  ASSERT_EQ(opt.get_parameters().size(), 3u);
  ASSERT_EQ(opt.get_arguments().size(), 5u);
}

TEST(ClibMisc, GetOptionsCtorInvalid) {
  ASSERT_TRUE(check_unknown_option());
  ASSERT_TRUE(check_require_argument());
}

TEST(ClibMisc, GetOptionsCopy) {
  std::vector<std::string> args;
  my_options ref(args);

  my_options opt1(ref);
  ASSERT_TRUE((ref == opt1));

  my_options opt2 = ref;
  ASSERT_EQ(ref, opt2);
}

TEST(ClibMisc, GetOptionsGetArgumentByLongName) {
  ASSERT_TRUE(invalid_long_name());
  ASSERT_TRUE(valid_long_name());
}

TEST(ClibMisc, GetOptionsGetArgumentByName) {
  ASSERT_TRUE(invalid_name());
  ASSERT_TRUE(valid_name());
}

TEST(ClibMisc, StringifierAppend) {
  {
    stringifier buffer;
    buffer.append(__FILE__, sizeof(__FILE__));
    ASSERT_FALSE(strcmp(buffer.data(), __FILE__));
  }

  {
    stringifier buffer;
    buffer.append(__FILE__, sizeof(__FILE__) - 3);
    ASSERT_FALSE(strncmp(buffer.data(), __FILE__, sizeof(__FILE__) - 3));
  }

  {
    char ref[] = "**\0**";
    stringifier buffer;
    buffer.append(ref, sizeof(ref));
    ASSERT_FALSE(memcmp(buffer.data(), ref, 3));
  }
}

TEST(ClibMisc, StringifierCtor) {
  stringifier buffer;
  ASSERT_FALSE(strcmp(buffer.data(), ""));
}

TEST(ClibMisc, StringifierCopy) {
  static char const message[] = "Centreon Clib";

  stringifier buffer;
  buffer << message;
  ASSERT_FALSE(strcmp(buffer.data(), message));
}

TEST(ClibMisc, StringifierBool) {
  stringifier buffer;
  buffer << false << true;
  ASSERT_FALSE(strcmp(buffer.data(), "falsetrue"));
}

TEST(ClibMisc, StringifierChar) {
  stringifier buffer;
  buffer << static_cast<char>(CHAR_MIN);
  buffer << static_cast<char>(CHAR_MAX);

  char ref[] = {CHAR_MIN, CHAR_MAX, 0};
  ASSERT_FALSE(strcmp(buffer.data(), ref));
}

TEST(ClibMisc, StringifierDouble) {
  ASSERT_TRUE(check_double(DBL_MIN));
  ASSERT_TRUE(check_double(DBL_MAX));
  ASSERT_TRUE(check_double(0.0));
  ASSERT_TRUE(check_double(1.1));
  ASSERT_TRUE(check_double(-1.456657563));
}

TEST(ClibMisc, StringifierInt) {
  stringifier buffer;
  buffer << static_cast<int>(INT_MIN);
  buffer << static_cast<int>(INT_MAX);

  std::ostringstream oss;
  oss << INT_MIN << INT_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierLong) {
  stringifier buffer;
  buffer << static_cast<long>(LONG_MIN);
  buffer << static_cast<long>(LONG_MAX);

  std::ostringstream oss;
  oss << LONG_MIN << LONG_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierLongLong) {
  stringifier buffer;
  buffer << static_cast<long long>(LLONG_MIN);
  buffer << static_cast<long long>(LLONG_MAX);

  std::ostringstream oss;
  oss << LLONG_MIN << LLONG_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierManyData) {
  static unsigned int const nb_msg(1024);
  static unsigned int const size_msg(512);

  stringifier buffer;
  std::string msg(size_msg, '*');
  for (unsigned int i(0); i < nb_msg; ++i)
    buffer << msg.c_str();

  std::string ref(nb_msg * size_msg, '*');
  ASSERT_FALSE(strcmp(buffer.data(), ref.c_str()));
}

TEST(ClibMisc, StringifierPChar) {
  stringifier buffer;
  buffer << __FILE__;
  ASSERT_FALSE(strcmp(buffer.data(), __FILE__));
}

TEST(ClibMisc, StringifierPVoid) {
  stringifier buffer;
  buffer << &buffer;

  std::ostringstream oss;
  oss << &buffer;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierStdString) {
  std::string str(__FILE__);
  stringifier buffer;
  buffer << str;

  ASSERT_FALSE(strcmp(buffer.data(), str.c_str()));
}

TEST(ClibMisc, StringifierUint) {
  stringifier buffer;
  buffer << static_cast<unsigned int>(0);
  buffer << static_cast<unsigned int>(UINT_MAX);

  std::ostringstream oss;
  oss << 0 << UINT_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierULong) {
  stringifier buffer;
  buffer << static_cast<unsigned long>(0);
  buffer << static_cast<unsigned long>(ULONG_MAX);

  std::ostringstream oss;
  oss << 0 << ULONG_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierULongLong) {
  stringifier buffer;
  buffer << static_cast<unsigned long long>(0);
  buffer << static_cast<unsigned long long>(ULLONG_MAX);

  std::ostringstream oss;
  oss << 0 << ULLONG_MAX;
  ASSERT_FALSE(strcmp(buffer.data(), oss.str().c_str()));
}

TEST(ClibMisc, StringifierReset) {
  stringifier buffer;
  buffer << &buffer;
  buffer.reset();
  ASSERT_FALSE(strcmp(buffer.data(), ""));
}
