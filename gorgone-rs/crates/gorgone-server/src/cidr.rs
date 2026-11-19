//! IP network in CIDR notation, for the client allowlist.

use std::fmt;
use std::net::IpAddr;
use std::str::FromStr;

use serde::{Deserialize, Deserializer, Serialize, Serializer};

/// An IPv4 or IPv6 network written `address/prefix-length`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Cidr {
    addr: IpAddr,
    prefix: u8,
}

/// Bits of `addr`, left-aligned in 128 bits, and the width of its family.
fn bits(addr: IpAddr) -> (u128, u8) {
    match addr {
        IpAddr::V4(v4) => (u128::from(v4.to_bits()) << 96, 32),
        IpAddr::V6(v6) => (v6.to_bits(), 128),
    }
}

impl Cidr {
    /// Whether `ip` belongs to the network; never across address families.
    #[must_use]
    pub fn contains(&self, ip: &IpAddr) -> bool {
        if self.addr.is_ipv4() != ip.is_ipv4() {
            return false;
        }
        // A zero-length prefix matches the whole family (and cannot shift).
        let mask = u128::MAX
            .checked_shl(128 - u32::from(self.prefix))
            .unwrap_or(0);
        (bits(self.addr).0 ^ bits(*ip).0) & mask == 0
    }
}

impl FromStr for Cidr {
    type Err = String;

    fn from_str(text: &str) -> Result<Self, Self::Err> {
        let invalid = || format!("invalid network {text:?}: expected address/prefix-length");
        let (addr, prefix) = text.split_once('/').ok_or_else(invalid)?;
        let addr: IpAddr = addr.parse().map_err(|_| invalid())?;
        // Digits only: `u8::from_str` would also take a sign.
        if prefix.is_empty() || !prefix.bytes().all(|b| b.is_ascii_digit()) {
            return Err(invalid());
        }
        let prefix: u8 = prefix.parse().map_err(|_| invalid())?;
        if prefix > bits(addr).1 {
            return Err(invalid());
        }
        Ok(Self { addr, prefix })
    }
}

impl fmt::Display for Cidr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}/{}", self.addr, self.prefix)
    }
}

impl Serialize for Cidr {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for Cidr {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        String::deserialize(deserializer)?
            .parse()
            .map_err(serde::de::Error::custom)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn contains(net: &str, ip: &str) -> bool {
        net.parse::<Cidr>().unwrap().contains(&ip.parse().unwrap())
    }

    #[test]
    fn ipv4_networks() {
        assert!(contains("10.0.0.0/8", "10.255.1.2"));
        assert!(!contains("10.0.0.0/8", "11.0.0.1"));
        assert!(contains("192.168.1.128/25", "192.168.1.200"));
        assert!(!contains("192.168.1.128/25", "192.168.1.127"));
        assert!(contains("192.168.1.7/32", "192.168.1.7"));
        assert!(!contains("192.168.1.7/32", "192.168.1.6"));
        assert!(contains("0.0.0.0/0", "203.0.113.9"));
        // Host bits of the network address are ignored.
        assert!(contains("10.1.2.3/8", "10.9.9.9"));
    }

    #[test]
    fn ipv6_networks() {
        assert!(contains("::1/128", "::1"));
        assert!(!contains("::1/128", "::2"));
        assert!(contains("2001:db8::/32", "2001:db8:ffff::1"));
        assert!(!contains("2001:db8::/32", "2001:db9::1"));
        assert!(contains("::/0", "2001:db8::1"));
        assert!(contains("fe80::/10", "febf::1"));
        assert!(!contains("fe80::/10", "fec0::1"));
    }

    #[test]
    fn families_never_mix() {
        assert!(!contains("0.0.0.0/0", "::1"));
        assert!(!contains("::/0", "127.0.0.1"));
        assert!(!contains("::ffff:10.0.0.0/104", "10.0.0.1"));
    }

    #[test]
    fn malformed_networks_are_refused() {
        for text in [
            "",
            "10.0.0.0",
            "10.0.0.0/",
            "10.0.0.0/33",
            "::1/129",
            "10.0.0.0/+8",
            "10.0.0.0/8/8",
            "10.0.0/8",
            "example.org/8",
            " 10.0.0.0/8",
            "10.0.0.0/256",
        ] {
            assert!(text.parse::<Cidr>().is_err(), "{text:?} accepted");
        }
    }

    #[test]
    fn text_round_trip() {
        for text in ["10.0.0.0/8", "::1/128", "2001:db8::/32", "0.0.0.0/0"] {
            let net: Cidr = text.parse().unwrap();
            assert_eq!(net.to_string(), text);
            let json = serde_json::to_string(&net).unwrap();
            assert_eq!(json, format!("{text:?}"));
            assert_eq!(serde_json::from_str::<Cidr>(&json).unwrap(), net);
        }
        assert!(serde_json::from_str::<Cidr>("\"10.0.0.0\"").is_err());
    }
}
