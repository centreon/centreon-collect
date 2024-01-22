use std::cell::RefCell;
use std::collections::VecDeque;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use cxx::{CxxString, SharedPtr};

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
        type Engine;
        type Muxer;

        //fn new_engine() -> Box<Engine>;
        //fn add_muxer(self: &mut Engine, name: &CxxString) -> Rc<RefCell<Muxer>>;
        fn write(self: &mut Muxer, d: &SharedPtr<data>);
    }
}

pub struct Engine {
    muxers: Vec<Rc<RefCell<Muxer>>>,
    queue: VecDeque<SharedPtr<ffi::data>>,
}

impl Engine {
    pub fn publish(&mut self, d: &SharedPtr<ffi::data>) {
        self.queue.push_back(d.clone());
    }
}

pub struct Muxer {
    name: String,
    parent: Arc<Mutex<Engine>>,
    queue: Mutex<VecDeque<SharedPtr<ffi::data>>>,
}

impl Muxer {
    pub fn write(&mut self, d: &SharedPtr<ffi::data>) {
        let mut q = self.queue.lock().unwrap();
        q.push_back(d.clone());
    }

    pub fn publish(&mut self, d: &SharedPtr<ffi::data>) {
        let mut eng = self.parent.lock().unwrap();
        eng.publish(d);
    }
}

fn new_engine() -> Arc<Mutex<Engine>> {
    return Arc::new(Mutex::new(Engine {
        muxers: Vec::new(),
        queue: VecDeque::new(),
    }));
}

fn add_muxer(parent: &Arc<Mutex<Engine>>, name: &CxxString) -> Rc<RefCell<Muxer>> {
    let name_rs = match name.to_str() {
        Ok(s) => s,
        // The error message is thrown away, this error should not arrive.
        Err(_e) => "BrokerBadMuxerName",
    };
    let new_muxer = Muxer {
        name: String::from(name_rs),
        parent: parent.clone(),
        queue: Mutex::new(VecDeque::new()),
    };

    let mut eng = parent.lock().unwrap();
    eng.muxers.push(Rc::new(RefCell::new(new_muxer)));
    return eng.muxers.last().unwrap().clone();
}
