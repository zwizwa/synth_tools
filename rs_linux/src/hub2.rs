// ALSA, JACK, TCP MIDI hub and MIDI state machine host.
// This is heavily inspired by a2jmidid.

// Notes:
//
// - ALSA sequencer uses high level events.  We don't use those, as
//   the point is to write some generic MIDI code that also works on
//   bare bones platforms.
//
// - To debug events:

extern crate alsa;
use alsa::seq::{Addr, ClientIter, MidiEvent, PortCap, PortIter, PortType, Seq};
// use alsa::Direction;
use std::ffi::CString;
use std::{thread, time};

fn main() {
    println!("hub2.rs");
    // &*CString::new("hw").unwrap()
    let s = Seq::open(None, None /* DUPLEX */, false).unwrap();
    s.set_client_name(&*CString::new("hub2").unwrap()).unwrap();
    s.create_simple_port(
        &*CString::new("midi").unwrap(),
        PortCap::READ | PortCap::SUBS_READ | PortCap::WRITE | PortCap::SUBS_WRITE,
        PortType::MIDI_GENERIC,
    )
    .unwrap();

    // List clients and ports
    for c in ClientIter::new(&s) {
        println!("{:?}:", c);
        for p in PortIter::new(&s, c.get_client()) {
            println!("  {:?}", p);
        }
    }

    // Current hack: this will fail on tp build, but will work on
    // local machine build if midi bluepill is plugged in.
    let addr = Addr {
        client: 24,
        port: 0,
    };

    let midi_bufsize = 128;
    let mut codec = MidiEvent::new(midi_bufsize).unwrap();
    codec.reset_decode();
    codec.enable_running_status(true);

    // FIXME: protocol isn't very clear. find example, e.g. a2jmidid

    match s.get_any_port_info(addr) {
        Ok(pi) => {
            println!("port: {:?}", pi);
            let midi = [0xB0, 0, 0];
            let mut ev = match codec.encode(&midi) {
                Ok((3, Some(ev1))) => ev1,
                _ => panic!("internal error"),
            };
            ev.set_source(0);
            ev.set_subs();
            ev.set_direct();
            println!("ev {:?}", ev);
            loop {
                println!(".");
                thread::sleep(time::Duration::from_millis(1000));
                s.event_output(&mut ev).unwrap();
                s.drain_output().unwrap();
                // s.event_output_direct(&mut ev).unwrap();
            }
        }
        Err(_) => {
            println!("client {}, port {} not found", addr.client, addr.port);
        }
    }
}
