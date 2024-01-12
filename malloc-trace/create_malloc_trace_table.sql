-- this sql code lets you to create table where you can load data from /tmp/malloc-trace.csv
-- you have to connect to the bdd with this command line:
-- mysql -h 127.0.0.1 --local-infile=1 -D centreon_storage -u centreon -pcentreon
-- then you load data with  
--  load data local infile '/tmp/malloc-trace.csv' into table malloc_trace fields terminated by ';';

CREATE TABLE `centreon_storage`.`malloc_trace` (
  `funct_name` VARCHAR(10) NOT NULL,
  `thread_id` INT UNSIGNED NULL,
  `address` BIGINT UNSIGNED NULL,
  `size` INT UNSIGNED NULL,
  `call_stack` TEXT(65535) NULL,
  FULLTEXT INDEX `call_stack_ind` (`call_stack`) VISIBLE);
