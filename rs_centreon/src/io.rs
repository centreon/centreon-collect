use cxx::CxxString;
use std::error::Error;
use std::{mem, u16, u32, usize};

#[cxx::bridge]
mod ffi {
    #[namespace = "com::centreon::broker::io"]
    extern "Rust" {
        type Data;
    }
}

struct Data{}

const HEADER_SIZE: usize = mem::size_of::<u16>() * 2 + mem::size_of::<u32>() * 3;

pub fn parse_stream(data: &CxxString) -> Result<(), Box<dyn Error>> {
    let data_slice: &[u8] = data.as_bytes();
    let mut offset: usize = 0;
    let mut data_vector: Vec<u8> = vec![];

    parse_object(data_slice, &mut data_vector, &mut offset);

    // deserialization
    Ok(())
}

fn parse_object(data_slice: &[u8], data_vector: &mut Vec<u8>, offset: &mut usize) {
    while let Err(_) = validate_header(data_slice) {
        // log in the future
        *offset += 1;
    }
    let header_slice = &data_slice[*offset..(*offset + HEADER_SIZE)];

    *offset += HEADER_SIZE;

    let header = Header::new(&header_slice).unwrap();
    data_vector.extend_from_slice(
        &data_slice[*offset..*offset + (usize::try_from(header.size).unwrap())]
    );

    *offset += usize::try_from(header.size).unwrap();

    while header.size == 0xffff {
        parse_object(data_slice, data_vector, offset);
    }
}

fn validate_header(data: &[u8]) -> Result<(), Box<dyn Error>> {
    let checksum = u16::from_be_bytes(
        data[..mem::size_of::<u16>()].try_into().unwrap()
    );

    if checksum != compute_crc16(data) {
        return Err(String::from("Cannot validate header checksum").into());
    }

    Ok(())
}

fn compute_crc16(data: &[u8]) -> u16 {
    const CRC_TBL: [u16; 16] = [
        0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
        0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f,
    ];
    let mut crc: u16 = 0xffff;
    for c in data {
        let mut cc: u16 = *c as u16;
        crc = ((crc >> 4) & 0x0fff) ^ CRC_TBL[((crc ^ cc) & 15) as usize];
        cc >>= 4;
        crc = ((crc >> 4) & 0x0fff) ^ CRC_TBL[((crc ^ cc) & 15) as usize];
    }
    return !crc & 0xffff;
}

struct Header {
    checksum: u16,
    size: u16,
    event_type: u32,
    source_id: u32,
    destination_id: u32,
}

impl Header {
    fn new(data: &[u8]) -> Result<Self, Box<dyn Error>> {
        let mut offset: usize = 0;
        let checksum = u16::from_be_bytes(
            data[offset..(offset + mem::size_of::<u16>())].try_into()?
        );
        offset += mem::size_of::<u16>();

        let size = u16::from_be_bytes(
            data[offset..(offset + mem::size_of::<u16>())].try_into()?
        );
        offset += mem::size_of::<u16>();

        let event_type = u32::from_be_bytes(
            data[offset..(offset + mem::size_of::<u32>())].try_into()?
        );
        offset += mem::size_of::<u32>();

        let source_id = u32::from_be_bytes(
            data[offset..(offset + mem::size_of::<u32>())].try_into()?
        );
        offset += mem::size_of::<u32>();

        let destination_id = u32::from_be_bytes(
            data[offset..(offset + mem::size_of::<u32>())].try_into()?
        );

        Ok(Header { checksum, size, event_type, source_id, destination_id })
    }
}
