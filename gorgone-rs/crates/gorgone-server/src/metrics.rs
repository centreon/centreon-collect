//! Process-wide metrics, rendered in the Prometheus text format. Every
//! series is declared here, with the values its label may take.

use std::fmt::Write;
use std::sync::atomic::{AtomicU64, Ordering::Relaxed};

use gorgone_core::kind;

use crate::store::state;

/// A counter or a gauge with one series per value of its label (a single
/// unlabelled series when `label` is empty).
pub(crate) struct Family<const N: usize> {
    name: &'static str,
    gauge: bool,
    label: &'static str,
    values: [&'static str; N],
    cells: [AtomicU64; N],
}

impl<const N: usize> Family<N> {
    const fn new(
        name: &'static str,
        gauge: bool,
        label: &'static str,
        values: [&'static str; N],
    ) -> Self {
        Self {
            name,
            gauge,
            label,
            values,
            cells: [const { AtomicU64::new(0) }; N],
        }
    }

    const fn labelled(name: &'static str, label: &'static str, values: [&'static str; N]) -> Self {
        Self::new(name, false, label, values)
    }

    /// Adds `count` to the series of `value`; an undeclared value is ignored.
    pub(crate) fn add(&self, value: &str, count: u64) {
        let cell = self.values.iter().position(|known| *known == value);
        debug_assert!(cell.is_some(), "{}: undeclared label {value}", self.name);
        if let Some(cell) = cell {
            self.cells[cell].fetch_add(count, Relaxed);
        }
    }

    fn render(&self, out: &mut String) {
        let kind = if self.gauge { "gauge" } else { "counter" };
        let _ = writeln!(out, "# TYPE {} {kind}", self.name);
        for (value, cell) in self.values.iter().zip(&self.cells) {
            let count = cell.load(Relaxed);
            let _ = if self.label.is_empty() {
                writeln!(out, "{} {count}", self.name)
            } else {
                writeln!(out, "{}{{{}=\"{value}\"}} {count}", self.name, self.label)
            };
        }
    }
}

impl Family<1> {
    const fn counter(name: &'static str) -> Self {
        Self::new(name, false, "", [""])
    }

    const fn gauge(name: &'static str) -> Self {
        Self::new(name, true, "", [""])
    }

    /// Adds `count` to an unlabelled counter.
    pub(crate) fn inc(&self, count: u64) {
        self.cells[0].fetch_add(count, Relaxed);
    }

    /// Sets an unlabelled gauge.
    pub(crate) fn set(&self, value: u64) {
        self.cells[0].store(value, Relaxed);
    }
}

/// Upper bounds of the batch size histograms; a last bucket is unbounded.
const BATCH_BOUNDS: [u64; 9] = [1, 2, 4, 8, 16, 32, 64, 128, 256];

/// Histogram of batch sizes.
pub(crate) struct Histogram {
    name: &'static str,
    buckets: [AtomicU64; BATCH_BOUNDS.len() + 1],
    sum: AtomicU64,
}

impl Histogram {
    const fn new(name: &'static str) -> Self {
        Self {
            name,
            buckets: [const { AtomicU64::new(0) }; BATCH_BOUNDS.len() + 1],
            sum: AtomicU64::new(0),
        }
    }

    /// Records one observation.
    pub(crate) fn record(&self, value: u64) {
        let bucket = BATCH_BOUNDS.partition_point(|bound| *bound < value);
        self.buckets[bucket].fetch_add(1, Relaxed);
        self.sum.fetch_add(value, Relaxed);
    }

    fn render(&self, out: &mut String) {
        let name = self.name;
        let _ = writeln!(out, "# TYPE {name} histogram");
        let mut count = 0;
        for (bucket, cell) in self.buckets.iter().enumerate() {
            // Buckets are cumulative in the exposition format.
            count += cell.load(Relaxed);
            let _ = match BATCH_BOUNDS.get(bucket) {
                Some(bound) => writeln!(out, "{name}_bucket{{le=\"{bound}\"}} {count}"),
                None => writeln!(out, "{name}_bucket{{le=\"+Inf\"}} {count}"),
            };
        }
        let _ = writeln!(out, "{name}_sum {}", self.sum.load(Relaxed));
        let _ = writeln!(out, "{name}_count {count}");
    }
}

pub(crate) static AGENT_SESSIONS: Family<1> = Family::gauge("gorgone_agent_sessions");
pub(crate) static AGENT_SESSIONS_STALLED: Family<1> =
    Family::counter("gorgone_agent_sessions_stalled_total");
pub(crate) static JOBS_SUBMITTED: Family<4> = Family::labelled(
    "gorgone_jobs_submitted_total",
    "kind",
    [
        kind::RUN_COMMANDS,
        kind::ENGINE_COMMANDS,
        kind::APPLY_CONFIG,
        kind::SERVICE_CONTROL,
    ],
);
pub(crate) static JOBS_DISPATCHED: Family<1> = Family::counter("gorgone_jobs_dispatched_total");
pub(crate) static JOBS_FINISHED: Family<4> = Family::labelled(
    "gorgone_jobs_finished_total",
    "state",
    [
        state::SUCCEEDED,
        state::FAILED,
        state::EXPIRED,
        state::CANCELLED,
    ],
);
pub(crate) static JOB_EVENTS: Family<1> = Family::counter("gorgone_job_events_total");
pub(crate) static JOB_EVENTS_DROPPED: Family<1> =
    Family::counter("gorgone_job_events_dropped_total");
pub(crate) static EVENT_BATCH_SIZE: Histogram = Histogram::new("gorgone_event_batch_size");
pub(crate) static SUBMIT_BATCH_SIZE: Histogram = Histogram::new("gorgone_submit_batch_size");
pub(crate) static CENTCORE_JOBS: Family<1> = Family::counter("gorgone_centcore_jobs_total");
pub(crate) static CENTCORE_FILES_REFUSED: Family<1> =
    Family::counter("gorgone_centcore_files_refused_total");
pub(crate) static CENTCORE_DROPPED: Family<5> = Family::labelled(
    "gorgone_centcore_dropped_total",
    "reason",
    [
        "malformed",
        "refused",
        "unknown",
        "unknown_node",
        "unsafe_source",
    ],
);
pub(crate) static CENTCORE_UNSUPPORTED: Family<2> = Family::labelled(
    "gorgone_centcore_unsupported_total",
    "command",
    ["SENDEXPORTFILE", "STARTWORKER"],
);

/// Every metric, in the Prometheus text exposition format.
pub(crate) fn render() -> String {
    let mut out = String::new();
    AGENT_SESSIONS.render(&mut out);
    AGENT_SESSIONS_STALLED.render(&mut out);
    JOBS_SUBMITTED.render(&mut out);
    JOBS_DISPATCHED.render(&mut out);
    JOBS_FINISHED.render(&mut out);
    JOB_EVENTS.render(&mut out);
    JOB_EVENTS_DROPPED.render(&mut out);
    EVENT_BATCH_SIZE.render(&mut out);
    SUBMIT_BATCH_SIZE.render(&mut out);
    CENTCORE_JOBS.render(&mut out);
    CENTCORE_FILES_REFUSED.render(&mut out);
    CENTCORE_DROPPED.render(&mut out);
    CENTCORE_UNSUPPORTED.render(&mut out);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn families_render_one_series_per_label_value() {
        let plain = Family::counter("plain_total");
        plain.inc(2);
        plain.inc(1);
        let labelled = Family::labelled("labelled_total", "state", ["a", "b"]);
        labelled.add("b", 5);
        let gauge = Family::gauge("level");
        gauge.set(7);
        gauge.set(4);
        let mut out = String::new();
        plain.render(&mut out);
        labelled.render(&mut out);
        gauge.render(&mut out);
        assert_eq!(
            out,
            "# TYPE plain_total counter\nplain_total 3\n\
             # TYPE labelled_total counter\n\
             labelled_total{state=\"a\"} 0\nlabelled_total{state=\"b\"} 5\n\
             # TYPE level gauge\nlevel 4\n"
        );
    }

    #[test]
    fn histogram_buckets_are_cumulative() {
        let histogram = Histogram::new("batch");
        for value in [1, 2, 3, 300] {
            histogram.record(value);
        }
        let mut out = String::new();
        histogram.render(&mut out);
        assert!(out.starts_with("# TYPE batch histogram\n"), "{out}");
        for line in [
            "batch_bucket{le=\"1\"} 1\n",
            "batch_bucket{le=\"2\"} 2\n",
            "batch_bucket{le=\"4\"} 3\n",
            "batch_bucket{le=\"256\"} 3\n",
            "batch_bucket{le=\"+Inf\"} 4\n",
            "batch_sum 306\n",
            "batch_count 4\n",
        ] {
            assert!(out.contains(line), "{line} not in {out}");
        }
    }

    #[test]
    fn every_metric_is_exposed() {
        let out = render();
        for name in [
            "gorgone_agent_sessions ",
            "gorgone_jobs_submitted_total{kind=\"run_commands\"}",
            "gorgone_jobs_finished_total{state=\"expired\"}",
            "gorgone_centcore_dropped_total{reason=\"unsafe_source\"}",
            "gorgone_submit_batch_size_count",
        ] {
            assert!(out.contains(name), "{name} not in {out}");
        }
    }
}
