//! BBDO framing over a byte stream (`broker/core/bbdo/stream.cc`): a 16-byte
//! header (CRC-16, payload size, event type, source and destination ids, in
//! network byte order) before each part of at most 0xFFFF bytes.

/// Size of a packet header (`BBDO_HEADER_SIZE`).
pub const HEADER_SIZE: usize = 16;
/// Largest payload of one packet; a payload this size announces a next part.
const MAX_PART: usize = 0xFFFF;

/// Event categories (`bbdo/events.hh`).
pub mod category {
    /// Monitoring events.
    pub const NEB: u32 = 1;
    /// Protocol messages.
    pub const BBDO: u32 = 2;
}

/// Event types: `category << 16 | element` (`bbdo/events.hh`).
pub mod event {
    use super::category::{BBDO, NEB};

    /// `bbdo::de_version_response`, the welcome below BBDO 3.0.1.
    pub const VERSION_RESPONSE: u32 = (BBDO << 16) | 1;
    /// `bbdo::de_ack`, the acknowledgement below BBDO 3.0.1.
    pub const ACK: u32 = (BBDO << 16) | 2;
    /// `bbdo::de_welcome`.
    pub const WELCOME: u32 = (BBDO << 16) | 7;
    /// `bbdo::de_pb_ack`.
    pub const PB_ACK: u32 = (BBDO << 16) | 8;
    /// `bbdo::de_pb_stop`.
    pub const PB_STOP: u32 = (BBDO << 16) | 9;
    /// `neb::de_pb_service`.
    pub const SERVICE: u32 = (NEB << 16) | 27;
    /// `neb::de_pb_service_status`.
    pub const SERVICE_STATUS: u32 = (NEB << 16) | 29;
    /// `neb::de_pb_host`.
    pub const HOST: u32 = (NEB << 16) | 30;
    /// `neb::de_pb_host_status`.
    pub const HOST_STATUS: u32 = (NEB << 16) | 32;
    /// `neb::de_pb_instance_status`.
    pub const INSTANCE_STATUS: u32 = (NEB << 16) | 42;
    /// `neb::de_pb_instance`.
    pub const INSTANCE: u32 = (NEB << 16) | 44;
    /// `neb::de_pb_instance_configuration`.
    pub const INSTANCE_CONFIGURATION: u32 = (NEB << 16) | 54;
    /// `neb::de_pb_comment`.
    pub const COMMENT: u32 = (NEB << 16) | 35;
    /// `neb::de_pb_downtime`.
    pub const DOWNTIME: u32 = (NEB << 16) | 36;
    /// `neb::de_pb_acknowledgement`.
    pub const ACKNOWLEDGEMENT: u32 = (NEB << 16) | 45;
}

const CRC_TABLE: [u16; 16] = [
    0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387, 0x8408, 0x9489, 0xa50a, 0xb58b,
    0xc60c, 0xd68d, 0xe70e, 0xf78f,
];

/// `misc::crc16_ccitt` (`broker/core/src/misc/misc.cc`): CRC-16/X-25.
#[must_use]
pub fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0xffff;
    for &byte in data {
        let mut c = byte;
        crc = ((crc >> 4) & 0x0fff) ^ CRC_TABLE[usize::from((crc ^ u16::from(c)) & 15)];
        c >>= 4;
        crc = ((crc >> 4) & 0x0fff) ^ CRC_TABLE[usize::from((crc ^ u16::from(c)) & 15)];
    }
    !crc
}

fn push_header(out: &mut Vec<u8>, event_type: u32, size: usize) {
    let start = out.len();
    out.extend_from_slice(&[0, 0]);
    // `size` is at most MAX_PART.
    out.extend_from_slice(&u16::try_from(size).unwrap_or(u16::MAX).to_be_bytes());
    out.extend_from_slice(&event_type.to_be_bytes());
    out.extend_from_slice(&[0; 8]);
    let crc = crc16(&out[start + 2..start + HEADER_SIZE]);
    out[start..start + 2].copy_from_slice(&crc.to_be_bytes());
}

/// Appends the packets of one event to `out`, cut as `stream::serialize`
/// cuts a protobuf event: parts of 0xFFFF bytes, then the rest. Unlike
/// `serialize`, a payload of an exact multiple of 0xFFFF bytes ends with an
/// empty part, which the C++ reader glues like any last part: without it the
/// reader would wait for a part that never comes.
pub fn encode(event_type: u32, payload: &[u8], out: &mut Vec<u8>) {
    let mut rest = payload;
    loop {
        if rest.len() < MAX_PART {
            push_header(out, event_type, rest.len());
            out.extend_from_slice(rest);
            return;
        }
        push_header(out, event_type, MAX_PART);
        out.extend_from_slice(&rest[..MAX_PART]);
        rest = &rest[MAX_PART..];
    }
}

/// Encodes a protobuf message as one event.
#[must_use]
pub fn frame(event_type: u32, message: &impl prost::Message) -> Vec<u8> {
    let payload = message.encode_to_vec();
    let mut out = Vec::with_capacity(payload.len() + HEADER_SIZE);
    encode(event_type, &payload, &mut out);
    out
}

/// Cuts a byte stream into events, as `stream::_read_any` does: a header
/// with a bad CRC makes it skip one byte; parts are glued back together.
#[derive(Debug, Default)]
pub struct Decoder {
    buffer: Vec<u8>,
    /// Payload of an event whose last part has not come yet.
    partial: Option<(u32, Vec<u8>)>,
    /// Bytes skipped because of bad checksums.
    pub skipped: usize,
}

impl Decoder {
    /// Adds received bytes.
    pub fn push(&mut self, data: &[u8]) {
        self.buffer.extend_from_slice(data);
    }

    /// Next complete event: its type and payload.
    pub fn next_event(&mut self) -> Option<(u32, Vec<u8>)> {
        loop {
            if self.buffer.len() < HEADER_SIZE {
                return None;
            }
            let header = &self.buffer[..HEADER_SIZE];
            let crc = u16::from_be_bytes([header[0], header[1]]);
            if crc != crc16(&header[2..]) {
                self.buffer.remove(0);
                self.skipped += 1;
                continue;
            }
            let size = usize::from(u16::from_be_bytes([header[2], header[3]]));
            let event_type = u32::from_be_bytes([header[4], header[5], header[6], header[7]]);
            if self.buffer.len() < HEADER_SIZE + size {
                return None;
            }
            let part: Vec<u8> = self
                .buffer
                .drain(..HEADER_SIZE + size)
                .skip(HEADER_SIZE)
                .collect();
            let mut payload = match self.partial.take() {
                Some((partial_type, mut head)) if partial_type == event_type => {
                    head.extend_from_slice(&part);
                    head
                }
                _ => part,
            };
            if size == MAX_PART {
                self.partial = Some((event_type, std::mem::take(&mut payload)));
                continue;
            }
            return Some((event_type, payload));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn hex(bytes: &[u8]) -> String {
        bytes.iter().map(|byte| format!("{byte:02x}")).collect()
    }

    /// Headers printed by the CRC function of `broker/core/src/misc/misc.cc`
    /// compiled as it is, with the header layout of `stream::serialize`.
    #[test]
    fn headers_match_the_cpp_code() {
        assert_eq!(crc16(b"123456789"), 0x906e);
        for (event_type, size, expected) in [
            (event::WELCOME, 23, "a9390017000200070000000000000000"),
            (event::INSTANCE, 10, "6ddf000a0001002c0000000000000000"),
            (
                event::SERVICE_STATUS,
                0xffff,
                "5828ffff0001001d0000000000000000",
            ),
            (event::PB_ACK, 2, "76d80002000200080000000000000000"),
        ] {
            let mut out = Vec::new();
            push_header(&mut out, event_type, size);
            assert_eq!(hex(&out), expected, "{event_type:#x}");
        }
    }

    /// Header of the welcome a real cbd (broker image of this branch) sent
    /// to the scheduler, captured by `tests/real_cbd.rs`: cbd sets the source
    /// id to its poller id.
    #[test]
    fn header_written_by_cbd_is_accepted() {
        let header: [u8; HEADER_SIZE] = [
            0x2c, 0x01, 0x00, 0x3b, 0x00, 0x02, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
            0x00, 0x00,
        ];
        assert_eq!(crc16(&header[2..]), 0x2c01);
        let mut decoder = Decoder::default();
        decoder.push(&header);
        decoder.push(&[0u8; 0x3b]);
        let (event_type, payload) = decoder.next_event().unwrap();
        assert_eq!((event_type, payload.len()), (event::WELCOME, 0x3b));
        assert_eq!(decoder.skipped, 0);
    }

    #[test]
    fn round_trip_with_parts_and_garbage() {
        let small: Vec<u8> = (0..10).collect();
        let big: Vec<u8> = (0..200_000u32).map(|i| (i % 251) as u8).collect();
        let exact = vec![7u8; MAX_PART];
        let mut stream = vec![0xde, 0xad];
        encode(event::INSTANCE, &small, &mut stream);
        encode(event::SERVICE_STATUS, &big, &mut stream);
        encode(event::HOST, &exact, &mut stream);
        encode(event::PB_ACK, &[], &mut stream);
        // 200 000 bytes: three full parts and a fourth one.
        assert_eq!(
            stream.len(),
            2 + 26 + 200_000 + 4 * 16 + 2 * 16 + MAX_PART + 16
        );

        let mut decoder = Decoder::default();
        let mut events = Vec::new();
        // Fed in uneven slices, as TCP delivers them.
        for chunk in stream.chunks(7777) {
            decoder.push(chunk);
            while let Some(event) = decoder.next_event() {
                events.push(event);
            }
        }
        assert_eq!(decoder.skipped, 2);
        assert_eq!(events.len(), 4);
        assert_eq!(events[0], (event::INSTANCE, small));
        assert_eq!(events[1], (event::SERVICE_STATUS, big));
        assert_eq!(events[2], (event::HOST, exact));
        assert_eq!(events[3], (event::PB_ACK, Vec::new()));
    }
}
