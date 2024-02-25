// ALSA, JACK, TCP MIDI hub and MIDI state machine host.
// This is heavily inspired by a2jmidid.

extern crate alsa;
use alsa::seq::{Addr, Seq};
// use alsa::Direction;
use std::ffi::CString;

fn main() {
    println!("hub2.rs");
    // &*CString::new("hw").unwrap()
    let s = Seq::open(None, None /* DUPLEX */, false).unwrap();
    s.set_client_name(&*CString::new("hub2").unwrap()).unwrap();

    let a = Addr { client: 0, port: 0 };
    let i = s.get_any_port_info(a).unwrap();
    // It has fmt::Debug trait
    println!("port: {:?}", i);
}
