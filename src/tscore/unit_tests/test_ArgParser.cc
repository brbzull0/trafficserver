/** @file

  Unit test for ArgParser

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include <catch2/catch_test_macros.hpp>
#include "tscore/ArgParser.h"

int           global;
ts::ArgParser parser;
ts::ArgParser parser2;

TEST_CASE("Parsing test", "[parse]")
{
  // initialize and construct the parser
  parser.add_global_usage("traffic_blabla [--SWITCH]");

  setenv("ENV_TEST", "env_test", 0);
  setenv("ENV_TEST2", "env_test2", 0);
  parser.add_option("--globalx", "-x", "global switch x", "ENV_TEST", 2, "", "globalx_key");
  parser.add_option("--globaly", "-y", "global switch y", "", 2, "default1 default2");
  parser.add_option("--globalz", "-z", "global switch z", "", MORE_THAN_ONE_ARG_N);

  ts::ArgParser::Command &init_command   = parser.add_command("init", "initialize traffic blabla", "ENV_TEST2", 1, nullptr);
  ts::ArgParser::Command &remove_command = parser.add_command("remove", "remove traffic blabla");

  init_command.add_option("--initoption", "-i", "init option");
  init_command.add_option("--initoption2", "-j", "init2 option", "", 1, "");
  init_command.add_command("subinit", "sub initialize traffic blabla", "", 2, nullptr, "subinit_key")
    .add_option("--subinitopt", "-s", "sub init option");

  remove_command.add_command("subremove", "sub remove traffic blabla").add_command("subsubremove", "sub sub remove");

  ts::Arguments parsed_data;

  // first run of arguments
  const char *argv1[] = {"traffic_blabla", "init", "a", "--initoption", "--globalx", "x", "y", nullptr};
  parsed_data         = parser.parse(argv1);

  REQUIRE(parsed_data.get("init") == true);
  REQUIRE(parsed_data.get("init").env() == "env_test2");
  REQUIRE(parsed_data.get("globalx_key") == true);
  REQUIRE(parsed_data.get("globalx_key").env() == "env_test");
  REQUIRE(parsed_data.get("globaly") == true);
  REQUIRE(parsed_data.get("globaly").size() == 2);
  REQUIRE(parsed_data.get("globaly").value() == "default1");
  REQUIRE(parsed_data.get("globaly").at(1) == "default2");
  REQUIRE(parsed_data.get("initoption") == true);
  REQUIRE(parsed_data.get("a") == false);
  REQUIRE(parsed_data.get("init").env().size() != 0);
  REQUIRE(parsed_data.get("init").size() == 1);
  REQUIRE(parsed_data.get("init").at(0) == "a");
  REQUIRE(parsed_data.get("globalx_key").size() == 2);
  REQUIRE(parsed_data.get("globalx_key").value() == "x");
  REQUIRE(parsed_data.get("globalx_key")[1] == "y");

  // second run of arguments
  const char *argv2[] = {"traffic_blabla",    "init",         "i",  "subinit", "a",  "b",
                         "--initoption2=abc", "--subinitopt", "-y", "y1",      "y2", nullptr};

  parsed_data = parser.parse(argv2);
  REQUIRE(parsed_data.get("init") == true);
  REQUIRE(parsed_data.get("subinitopt") == true);
  REQUIRE(parsed_data.get("globaly") == true);
  REQUIRE(parsed_data.get("globaly").size() == 2);
  REQUIRE(parsed_data.get("globaly")[0] == "y1");
  REQUIRE(parsed_data.get("globaly")[1] == "y2");
  REQUIRE(parsed_data.get("subinit_key").size() == 2);
  REQUIRE(parsed_data.get("subinit").size() == false);
  REQUIRE(parsed_data.get("initoption2").size() == 1);
  REQUIRE(parsed_data.get("initoption2")[0] == "abc");

  // third run of arguments
  const char *argv3[] = {"traffic_blabla", "-x",           "abc",          "xyz",          "remove", "subremove",
                         "subsubremove",   "--globalz=z1", "--globalz=z2", "--globalz=z3", nullptr};

  parsed_data = parser.parse(argv3);
  REQUIRE(parsed_data.has_action() == false);
  REQUIRE(parsed_data.get("remove") == true);
  REQUIRE(parsed_data.get("subremove") == true);
  REQUIRE(parsed_data.get("subsubremove") == true);
  REQUIRE(parsed_data.get("globalx_key").size() == 2);
  REQUIRE(parsed_data.get("globalz").size() == 3);
}

void
test_method_1()
{
  global = 0;
  parser2.set_error("error");
  return;
}

void
test_method_2(int num)
{
  if (num == 1) {
    global = 1;
  } else {
    global = 2;
  }
}

TEST_CASE("Invoke test", "[invoke]")
{
  int num = 1;

  parser2.add_global_usage("traffic_blabla [--SWITCH]");
  // function by reference
  parser2.add_command("func", "some test function 1", "", 0, &test_method_1);
  // lambda
  parser2.add_command("func2", "some test function 2", "", 0, [&]() { return test_method_2(num); });

  ts::Arguments parsed_data;

  const char *argv1[] = {"traffic_blabla", "func", nullptr};

  parsed_data = parser2.parse(argv1);
  REQUIRE(parsed_data.has_action() == true);
  parsed_data.invoke();
  REQUIRE(global == 0);
  REQUIRE(parser2.get_error() == "error");

  const char *argv2[] = {"traffic_blabla", "func2", nullptr};

  parsed_data = parser2.parse(argv2);
  parsed_data.invoke();
  REQUIRE(global == 1);
  num = 3;
  parsed_data.invoke();
  REQUIRE(global == 2);
}

TEST_CASE("Case sensitive short options", "[parse]")
{
  ts::ArgParser cs_parser;
  cs_parser.add_global_usage("test_prog [--SWITCH]");

  // Add a command with two options that differ only in case: -t and -T
  ts::ArgParser::Command &cmd = cs_parser.add_command("process", "process data");
  cmd.add_option("--tag", "-t", "a label", "", 1, "");
  cmd.add_option("--threshold", "-T", "a numeric value", "", 1, "100");

  ts::Arguments parsed;

  // Use lowercase -t: should set "tag" only
  const char *argv1[] = {"test_prog", "process", "-t", "my_tag", nullptr};
  parsed              = cs_parser.parse(argv1);
  REQUIRE(parsed.get("tag") == true);
  REQUIRE(parsed.get("tag").value() == "my_tag");
  // threshold should still have its default
  REQUIRE(parsed.get("threshold").value() == "100");

  // Use uppercase -T: should set "threshold" only
  const char *argv2[] = {"test_prog", "process", "-T", "200", nullptr};
  parsed              = cs_parser.parse(argv2);
  REQUIRE(parsed.get("threshold") == true);
  REQUIRE(parsed.get("threshold").value() == "200");
  // tag should be empty (no default)
  REQUIRE(parsed.get("tag").value() == "");

  // Use both -t and -T together
  const char *argv3[] = {"test_prog", "process", "-t", "foo", "-T", "500", nullptr};
  parsed              = cs_parser.parse(argv3);
  REQUIRE(parsed.get("tag") == true);
  REQUIRE(parsed.get("tag").value() == "foo");
  REQUIRE(parsed.get("threshold") == true);
  REQUIRE(parsed.get("threshold").value() == "500");
}

TEST_CASE("with_required does not trigger on default values", "[parse]")
{
  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  ts::ArgParser::Command &cmd = parser.add_command("scan", "scan targets");
  cmd.add_option("--tag", "-t", "a label", "", 1, "");
  cmd.add_option("--verbose", "-v", "enable verbose output");
  cmd.add_option("--threshold", "-T", "a numeric value", "", 1, "100").with_required("--verbose");

  // -t alone should NOT trigger --threshold's dependency on --verbose.
  // The default value "100" for --threshold must not count as "explicitly used".
  const char   *argv1[] = {"test_prog", "scan", "-t", "my_tag", nullptr};
  ts::Arguments parsed  = parser.parse(argv1);
  REQUIRE(parsed.get("tag").value() == "my_tag");
  // threshold default should still be applied after validation
  REQUIRE(parsed.get("threshold").value() == "100");

  // -T with -v should work fine
  const char *argv2[] = {"test_prog", "scan", "-T", "200", "-v", nullptr};
  parsed              = parser.parse(argv2);
  REQUIRE(parsed.get("threshold").value() == "200");
  REQUIRE(parsed.get("verbose") == true);

  // -t and -T together with -v should work
  const char *argv3[] = {"test_prog", "scan", "-t", "foo", "-T", "300", "-v", nullptr};
  parsed              = parser.parse(argv3);
  REQUIRE(parsed.get("tag").value() == "foo");
  REQUIRE(parsed.get("threshold").value() == "300");
  REQUIRE(parsed.get("verbose") == true);
}

TEST_CASE("An environment variable supplies the value of an option used without one", "[parse][env]")
{
  setenv("TS_TEST_FILE", "/tmp/from_env.yaml", 1);
  setenv("TS_TEST_TAGS", "http dns", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  ts::ArgParser::Command &cmd = parser.add_command("get", "get records", "", MORE_THAN_ONE_ARG_N, nullptr);
  cmd.add_option("--file", "-f", "read from a file", "TS_TEST_FILE", 1);
  cmd.add_option("--tags", "-t", "debug tags", "TS_TEST_TAGS", 2);

  // The option is used with no value of its own, which used to be a usage error for a
  // fixed number of arguments. The variable supplies it instead.
  const char   *argv1[] = {"test_prog", "get", "proxy.config.x", "-f", nullptr};
  ts::Arguments parsed  = parser.parse(argv1);
  REQUIRE(parsed.get("file") == true);
  REQUIRE(parsed.get("file").value() == "/tmp/from_env.yaml");
  REQUIRE(parsed.get("get").size() == 1);
  // The caller can tell the user where a value it did not type came from.
  REQUIRE(parsed.get("file").env_source() == "TS_TEST_FILE");

  // A typed value always wins over the variable, and is not reported as coming from it.
  const char *argv2[] = {"test_prog", "get", "-f", "/tmp/typed.yaml", "proxy.config.x", nullptr};
  parsed              = parser.parse(argv2);
  REQUIRE(parsed.get("file").value() == "/tmp/typed.yaml");
  REQUIRE(parsed.get("file").env_source().empty());

  // An option taking several values splits the variable on spaces.
  const char *argv3[] = {"test_prog", "get", "proxy.config.x", "-t", nullptr};
  parsed              = parser.parse(argv3);
  REQUIRE(parsed.get("tags").size() == 2);
  REQUIRE(parsed.get("tags").at(0) == "http");
  REQUIRE(parsed.get("tags").at(1) == "dns");
}

TEST_CASE("An environment variable does not stand in for an option that was not used", "[parse][env]")
{
  setenv("TS_TEST_FILE", "/tmp/from_env.yaml", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  ts::ArgParser::Command &cmd = parser.add_command("get", "get records", "", MORE_THAN_ONE_ARG_N, nullptr);
  cmd.add_option("--file", "-f", "read from a file", "TS_TEST_FILE", 1);

  // Exporting the variable must not make the command behave as though the option was
  // given, because an option can select where the command takes its data from.
  const char   *argv[] = {"test_prog", "get", "proxy.config.x", nullptr};
  ts::Arguments parsed = parser.parse(argv);
  REQUIRE(parsed.get("file") == false);
  REQUIRE(parsed.get("file").size() == 0);
  REQUIRE(parsed.get("get").size() == 1);
}

TEST_CASE("An environment variable outranks a declared default but not a typed value", "[parse][env]")
{
  setenv("TS_TEST_THRESHOLD", "200", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  ts::ArgParser::Command &cmd = parser.add_command("scan", "scan targets");
  cmd.add_option("--threshold", "-T", "a numeric value", "TS_TEST_THRESHOLD", 1, "100");

  // Used with no value, so the variable is preferred over the default.
  const char   *argv1[] = {"test_prog", "scan", "-T", nullptr};
  ts::Arguments parsed  = parser.parse(argv1);
  REQUIRE(parsed.get("threshold").value() == "200");

  // Not used at all, so the default still applies and no variable is credited.
  const char *argv2[] = {"test_prog", "scan", nullptr};
  parsed              = parser.parse(argv2);
  REQUIRE(parsed.get("threshold").value() == "100");
  REQUIRE(parsed.get("threshold").env_source().empty());

  // Typed, so neither the variable nor the default is consulted.
  const char *argv3[] = {"test_prog", "scan", "-T", "300", nullptr};
  parsed              = parser.parse(argv3);
  REQUIRE(parsed.get("threshold").value() == "300");
}

TEST_CASE("A single value from an environment variable keeps its spaces", "[parse][env]")
{
  setenv("TS_TEST_PATH", "/tmp/a dir/records.yaml", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  ts::ArgParser::Command &cmd = parser.add_command("get", "get records", "", MORE_THAN_ONE_ARG_N, nullptr);
  cmd.add_option("--file", "-f", "read from a file", "TS_TEST_PATH", 1);

  const char   *argv[] = {"test_prog", "get", "proxy.config.x", "-f", nullptr};
  ts::Arguments parsed = parser.parse(argv);
  REQUIRE(parsed.get("file").size() == 1);
  REQUIRE(parsed.get("file").value() == "/tmp/a dir/records.yaml");
}

TEST_CASE("An empty environment variable is treated as unset", "[parse][env]")
{
  setenv("TS_TEST_EMPTY", "", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");

  // A variable arity, because with a fixed one a missing value that the environment cannot
  // supply is still a usage error.
  ts::ArgParser::Command &cmd = parser.add_command("scan", "scan targets");
  cmd.add_option("--threshold", "-T", "a numeric value", "TS_TEST_EMPTY", MORE_THAN_ZERO_ARG_N, "100");

  // Nothing to supply, so the declared default is used rather than an empty value.
  const char   *argv[] = {"test_prog", "scan", "-T", nullptr};
  ts::Arguments parsed = parser.parse(argv);
  REQUIRE(parsed.get("threshold").value() == "100");
}

TEST_CASE("An environment variable supplies the arguments of a command", "[parse][env]")
{
  setenv("TS_TEST_CMD_ARG", "from_env", 1);

  ts::ArgParser parser;
  parser.add_global_usage("test_prog [OPTIONS]");
  parser.add_command("init", "initialize", "TS_TEST_CMD_ARG", 1, nullptr);

  // The command needs one argument and was given none, so the variable provides it.
  const char   *argv1[] = {"test_prog", "init", nullptr};
  ts::Arguments parsed  = parser.parse(argv1);
  REQUIRE(parsed.get("init") == true);
  REQUIRE(parsed.get("init").value() == "from_env");

  // A typed argument still wins.
  const char *argv2[] = {"test_prog", "init", "typed", nullptr};
  parsed              = parser.parse(argv2);
  REQUIRE(parsed.get("init").value() == "typed");
}
