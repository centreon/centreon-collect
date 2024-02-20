use futures::future::join_all;
use std::borrow::BorrowMut;
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
        type EventQueue;
        type Muxer;

        //fn new_engine() -> Box<EventQueue>;
        //fn add_muxer(self: &mut EventQueue, name: &CxxString) -> Rc<RefCell<Muxer>>;
        fn write(self: &mut Muxer, d: &SharedPtr<data>);
    }
}

pub struct CacheFile {
    name: String,
}

impl CacheFile {
    fn add(&self, d: &SharedPtr<ffi::data>) {}
}

enum State {
    NotStarted,
    Running,
    Stopped,
}

pub struct Engine {
    state: State,
    cache_file: Arc<Mutex<CacheFile>>,
    event_queue: Arc<Mutex<EventQueue>>,
    unprocessed_events: usize,
}

pub struct EventQueue {
    muxers: Vec<Rc<RefCell<Muxer>>>,
    queue: VecDeque<SharedPtr<ffi::data>>,
}

impl Engine {
    pub fn publish(&mut self, d: &SharedPtr<ffi::data>) {
        if match self.state {
            State::Stopped => {
                self.cache_file.lock().unwrap().add(d);
                self.unprocessed_events += 1;
                false
            }
            State::Running => {
                self.event_queue.lock().unwrap().queue.push_back(d.clone());
                true
            }
            State::NotStarted => {
                self.event_queue.lock().unwrap().queue.push_back(d.clone());
                false
            }
        } {
            self.send_to_subscribers();
        }
    }

    pub fn publish_v(&self, v: &Vec<SharedPtr<ffi::data>>) {
        let queue = &mut self.event_queue.lock().unwrap().queue;
        for d in v.iter() {
            queue.push_back(d.clone());
        }
    }

    async fn send_to_subscribers(&mut self) {
        let borrowed_ev_queue = self.event_queue.clone();
        let event_queue = borrowed_ev_queue.lock().unwrap();
        let queue = Arc::new(event_queue.queue.to_owned());
        let mut f = Vec::new();
        for mm in event_queue.muxers.iter() {
            let muxer = Rc::clone(mm);
            f.push(Muxer::publish_vd(muxer, queue.clone()));
        }
        join_all(f).await;
    }
}

pub struct Muxer {
    name: String,
    parent: Arc<Engine>,
    queue: Mutex<VecDeque<SharedPtr<ffi::data>>>,
}

impl Muxer {
    pub fn write(&mut self, d: &SharedPtr<ffi::data>) {
        let mut q = self.queue.lock().unwrap();
        q.push_back(d.clone());
    }

    pub fn publish(&mut self, d: &SharedPtr<ffi::data>) {
        self.queue.lock().unwrap().push_back(d.clone());
    }

    pub async fn publish_vd(muxer: Rc<RefCell<Muxer>>, d: Arc<VecDeque<SharedPtr<ffi::data>>>) {
        for dd in d.iter() {
            let mut m = (*muxer).borrow_mut();
            m.publish(dd);
        }
    }
    //pub fn publish_v(&mut self, v: &Vec<SharedPtr<ffi::data>>) {
    //    let eng = self.parent.clone();
    //    eng.publish_v(v);
    //}
}

fn new_engine() -> Engine {
    Engine {
        state: State::NotStarted,
        cache_file: Arc::new(Mutex::new(CacheFile {
            name: String::from("Cache-tst"),
        })),
        unprocessed_events: 0,
        event_queue: Arc::new(Mutex::new(EventQueue {
            muxers: Vec::new(),
            queue: VecDeque::new(),
        })),
    }
}

fn add_muxer(parent: &Arc<Engine>, name: &CxxString) -> Rc<RefCell<Muxer>> {
    let name_rs = match name.to_str() {
        Ok(s) => s,
        // The error message is thrown away, this error should not arrive.
        Err(_e) => "BrokerBadMuxerName",
    };

    let e = parent.clone().event_queue.clone();
    let mut eq = e.lock().unwrap();
    eq.muxers.push(Rc::new(RefCell::new(Muxer {
        name: String::from(name_rs),
        parent: parent.clone(),
        queue: Mutex::new(VecDeque::new()),
    })));
    return eq.muxers.last().unwrap().clone();
}
