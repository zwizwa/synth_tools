// ALSA, JACK, TCP MIDI hub and MIDI state machine host.
// This is heavily inspired by a2jmidid.

extern crate alsa;
use alsa::seq::{ClientIter, PortIter, Seq};
// use alsa::Direction;
use std::ffi::CString;
use std::{thread, time};

fn main() {
    println!("hub2.rs");
    // &*CString::new("hw").unwrap()
    let s = Seq::open(None, None /* DUPLEX */, false).unwrap();
    s.set_client_name(&*CString::new("hub2").unwrap()).unwrap();

    let clients: Vec<_> = ClientIter::new(&s).collect();
    for a in &clients {
        let ports: Vec<_> = PortIter::new(&s, a.get_client()).collect();
        println!("{:?}:", a);
        for p in &ports {
            println!("  {:?}", p);
        }
    }

    // let a = Addr {
    //     client: 24,
    //     port: 0,
    // };
    // let i = s.get_any_port_info(a).unwrap();
    // println!("port: {:?}", i);

    //loop {
    println!(".");
    thread::sleep(time::Duration::from_millis(1000));
    //}
}
