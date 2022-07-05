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

/*
partition by t and 20 sub partitions by metric_d
*/
SELECT * FROM  create_hypertable('metrics', 't', 'metric_id', 20, chunk_time_interval => 86400000000);

/*continuous agregate*/
CREATE MATERIALIZED VIEW avg_minute_metric
WITH (timescaledb.continuous) AS
SELECT metric_id,
       time_bucket(INTERVAL '1 minute', t) AS bucket,
       AVG(int_val + double_val)
FROM metrics
GROUP BY metric_id, bucket;

SELECT add_continuous_aggregate_policy('avg_minute_metric',
    start_offset => INTERVAL '1 month',
    end_offset => INTERVAL '1 h',
    schedule_interval => INTERVAL '1 h');

CREATE MATERIALIZED VIEW avg_hour_metric
WITH (timescaledb.continuous) AS
SELECT metric_id,
       time_bucket(INTERVAL '1 hour', t) AS bucket,
       AVG(int_val + double_val)
FROM metrics
GROUP BY metric_id, bucket;

SELECT add_continuous_aggregate_policy('avg_hour_metric',
    start_offset => INTERVAL '1 month',
    end_offset => INTERVAL '1 h',
    schedule_interval => INTERVAL '1 h');