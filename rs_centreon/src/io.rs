use cxx::CxxVector;

#[cxx::bidge]
mod ffi {
    #[namespace = "com::centreon::broker::io"]
    unsafe extern "Rust" {
        type data;

        fn serialize(data: &CxxVector<char>) -> Rc<RefCell<Data>>;
        fn deserialize(data: &Rc<RefCell<Data>>) -> CxxVector<char>;
    }
}

struct Data {
}
fn serialize(data: &CxxVector<char>) -> Rc<RefCell<io::Data>> {}
fn deserialize(data: &Rc<RefCell<io::Data>>) -> CxxVector<char> {}
