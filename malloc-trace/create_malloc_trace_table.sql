-- Copyright 2024 Centreon
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--
-- For more information : contact@centreon.com
--


-- this sql code lets you to create table where you can load data from /tmp/malloc-trace.csv
-- you have to connect to the bdd with this command line:
-- mysql -h 127.0.0.1 --local-infile=1 -D centreon_storage -u centreon -pcentreon
-- then you load data with  
-- load data local infile '/tmp/malloc-trace.csv' into table malloc_trace fields terminated by ';';

CREATE TABLE `centreon_storage`.`malloc_trace` (
  `funct_name` VARCHAR(10) NOT NULL,
  `thread_id` INT UNSIGNED NULL,
  `address` BIGINT UNSIGNED NULL,
  `size` INT UNSIGNED NULL,
  `ms_timestamp` BIGINT UNSIGNED NULL,
  `call_stack` TEXT(65535) NULL,
  FULLTEXT INDEX `call_stack_ind` (`call_stack`) VISIBLE);
