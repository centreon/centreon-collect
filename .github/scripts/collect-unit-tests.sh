#!/bin/bash
set -e

#Test

tests/ut_broker --gtest_output=xml:ut_broker.xml
tests/ut_engine --gtest_output=xml:ut_engine.xml
tests/ut_clib --gtest_output=xml:ut_clib.xml
tests/ut_connector --gtest_output=xml:ut_connector.xml
tests/ut_common --gtest_output=xml:ut_common.xml
echo "----------------------------------------------------------   end of ut tests ------------------------------------------------"
