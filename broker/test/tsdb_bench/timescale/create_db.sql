CREATE DATABASE centreon;
SET search_path TO centreon;

CREATE EXTENSION IF NOT EXISTS timescaledb;
DROP TABLE IF EXISTS "metrics";

CREATE TABLE "metrics" (
    t TIMESTAMP WITHOUT TIME ZONE NOT NULL,
    host_id BIGINT NOT NULL,
    service_id BIGINT NOT NULL,
    metric_id BIGINT NOT NULL,
    int_val BIGINT,
    double_val DOUBLE PRECISION 
);

SELECT create_hypertable('metrics', 't', 'metric_id');