//! Per source address limit of the refused `Enroll` attempts: a token
//! bucket per address, in the memory of each replica.
//!
//! Only refused attempts take a token, so that agents asking again while
//! their enrolment waits (every few seconds, possibly many behind one
//! address) are never slowed down; once an address has no token left, every
//! attempt from it is refused before the store is read, until the bucket
//! fills again. Each replica counts on its own: behind a load balancer that
//! spreads the calls, an address gets up to one budget per replica, and
//! behind one that hides the source, every agent shares the address of the
//! balancer.

use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::{Mutex, PoisonError};
use std::time::{Duration, Instant};

/// Addresses tracked at most; past it, the full buckets are forgotten, then
/// everything if that is not enough (the limit then restarts from zero).
const MAX_TRACKED: usize = 10_000;

#[derive(Debug, Clone, Copy)]
struct Bucket {
    tokens: f64,
    at: Instant,
}

/// Token buckets of the source addresses.
#[derive(Debug)]
pub(crate) struct RateLimiter {
    /// Tokens given back per second.
    rate: f64,
    burst: f64,
    buckets: Mutex<HashMap<IpAddr, Bucket>>,
}

impl RateLimiter {
    /// `per_minute` tokens given back per minute, at most `burst` kept; a
    /// `per_minute` of 0 disables the limit.
    pub(crate) fn new(per_minute: u32, burst: u32) -> Option<Self> {
        (per_minute > 0).then(|| Self {
            rate: f64::from(per_minute) / 60.0,
            burst: f64::from(burst.max(1)),
            buckets: Mutex::new(HashMap::new()),
        })
    }

    fn refilled(&self, bucket: Bucket, now: Instant) -> Bucket {
        let elapsed = now.saturating_duration_since(bucket.at).as_secs_f64();
        Bucket {
            tokens: (bucket.tokens + elapsed * self.rate).min(self.burst),
            at: now,
        }
    }

    /// True when `address` may make an attempt now.
    pub(crate) fn allows(&self, address: IpAddr) -> bool {
        self.allows_at(address, Instant::now())
    }

    fn allows_at(&self, address: IpAddr, now: Instant) -> bool {
        let buckets = self.buckets.lock().unwrap_or_else(PoisonError::into_inner);
        buckets
            .get(&address)
            .is_none_or(|bucket| self.refilled(*bucket, now).tokens >= 1.0)
    }

    /// Takes a token from `address`: one of its attempts was refused.
    pub(crate) fn charge(&self, address: IpAddr) {
        self.charge_at(address, Instant::now());
    }

    fn charge_at(&self, address: IpAddr, now: Instant) {
        let mut buckets = self.buckets.lock().unwrap_or_else(PoisonError::into_inner);
        if buckets.len() >= MAX_TRACKED && !buckets.contains_key(&address) {
            buckets.retain(|_, bucket| self.refilled(*bucket, now).tokens < self.burst);
            if buckets.len() >= MAX_TRACKED {
                buckets.clear();
            }
        }
        let bucket = buckets.entry(address).or_insert(Bucket {
            tokens: self.burst,
            at: now,
        });
        let mut refilled = self.refilled(*bucket, now);
        refilled.tokens = (refilled.tokens - 1.0).max(0.0);
        *bucket = refilled;
    }

    /// Time until `address` gets a token back, for the error message.
    pub(crate) fn retry_after(&self) -> Duration {
        Duration::from_secs_f64(1.0 / self.rate)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn refused_attempts_exhaust_the_bucket_which_refills() {
        let limiter = RateLimiter::new(60, 3).unwrap();
        let (a, b): (IpAddr, IpAddr) = ("10.0.0.1".parse().unwrap(), "10.0.0.2".parse().unwrap());
        let start = Instant::now();
        for _ in 0..3 {
            assert!(limiter.allows_at(a, start));
            limiter.charge_at(a, start);
        }
        assert!(!limiter.allows_at(a, start));
        // Another address has its own bucket.
        assert!(limiter.allows_at(b, start));
        // One token per second comes back, up to the burst.
        assert!(limiter.allows_at(a, start + Duration::from_millis(1100)));
        limiter.charge_at(a, start + Duration::from_millis(1100));
        assert!(!limiter.allows_at(a, start + Duration::from_millis(1200)));
        assert!(limiter.allows_at(a, start + Duration::from_secs(60)));
        assert_eq!(limiter.retry_after(), Duration::from_secs(1));
    }

    #[test]
    fn disabled_and_bounded() {
        assert!(RateLimiter::new(0, 10).is_none());
        let limiter = RateLimiter::new(1, 1).unwrap();
        let now = Instant::now();
        for i in 0..(MAX_TRACKED as u32 + 5) {
            limiter.charge_at(IpAddr::from(i.to_be_bytes()), now);
        }
        let tracked = limiter.buckets.lock().unwrap().len();
        assert!(tracked <= MAX_TRACKED, "{tracked}");
    }
}
