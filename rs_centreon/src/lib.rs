use std::collections::VecDeque;

use cxx::{SharedPtr, CxxString};

//#[cxx::bridge(namespace = "com::centreon::broker")]
#[cxx::bridge]
mod ffi {
    #[namespace = "com::centreon::broker::io"]
    unsafe extern "C++" {
        include!("broker/core/inc/com/centreon/broker/io/data.hh");
        type data;

        fn create_event() -> SharedPtr<data>;
    }

    #[namespace = "com::centreon::broker::rsc"]
    extern "Rust" {
        type Muxer;

        fn push_event(self: &mut Muxer, d: &SharedPtr<data>);
        fn new_muxer(name: &CxxString) -> Box<Muxer>;
    }

}

pub struct Muxer {
    name: String,
    queue: VecDeque<SharedPtr<ffi::data>>,
}

impl Muxer {
    pub fn new(name: &str) -> Muxer {
        return Muxer {name: String::from(name), queue: VecDeque::new() };
    }

    pub fn push_event(&mut self, d: &SharedPtr<ffi::data>) {
        self.queue.push_back(d.clone());
    }
}

fn new_muxer(name: &CxxString) -> Box<Muxer> {
    let name_rs = match name.to_str() {
        Ok(s) => s,
        // The error message is thrown away, this error should not arrive.
        Err(_e) => "Broker",
    };
    return Box::new(Muxer::new(name_rs));
}
