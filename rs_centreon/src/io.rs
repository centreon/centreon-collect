use cxx::CxxVector;
use std::cell::RefCell;
use std::rc::Rc;
use std::vec::Vec;

#[cxx::bridge]
mod ffi {
    #[namespace = "com::centreon::broker::io"]
    extern "Rust" {
        type Data;

        //fn serialize(data: &CxxVector<char>) -> Rc<RefCell<Data>>;
        //fn deserialize(data: &Rc<RefCell<Data>>) -> CxxVector<char>;
    }
}

struct Data {}
fn serialize(data: &CxxVector<char>) -> Rc<RefCell<Data>> {
    return Rc::new(RefCell::new(Data {}));
}

fn deserialize(data: &Rc<RefCell<Data>>) -> Vec<char> {
    return Vec::new();
}
