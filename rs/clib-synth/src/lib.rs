/* Rust studio monolith.

   I am moving away from Erlang for audio work. It was a nice idea but
   in practice it turns out to be too hard to maintain.  Erlang will
   remain the inter-PC layer, coupling multiple instances of the Rust
   monolith.

   I also want to move away from jack as an active component of my
   workflow.  The Erlang experiments have shown that managing multiple
   processes, connections, restarts is a significant amount of
   development overhead.

   So I am done.  I will be writing a single Rust monolith to contain
   all the management code.

   The monolith will be hosted inside Pure Data (Pd) as an external
   library.

   The monolith will be internally multi-threaded to host background
   processing and parallel DSP processing threads.  For this, Rust
   primitives can be used.

   The monolith will hard-code most hardware setup.  Will not use jack
   midi.  It will handle ALSA MIDI internally, modeled after a2jmidid.

   Some requirements:

   - Fast restart, restore all state and connections.  This is mostly
     handled by Pd (state of number boxes etc),


   EDIT: That was an annoyed rant.  So now tone it down.


   Stick to the Erlang approach, but gradually make hub.c centralize
   functionality.  Make the Erlang part minimal.

*/

// Rust<->C interop playground.

// FIXME: Turn this into code that runs on the F103. Currently still
// exploring.

#![no_std] // don't link the Rust standard library
extern crate heapless;
extern crate panic_halt;

// In no_std setup we need to use this instead of std::slice
// https://doc.rust-lang.org/core/slice/fn.from_raw_parts_mut.html
use core::cmp::{Ord, Ordering, PartialEq};
use core::slice;
use heapless::binary_heap::{BinaryHeap, Min};
use heapless::Vec;

// Assume that u32 is C uint32_t etc... Is that ok?

// The same structure is used for a linear sequence of events, and a
// circular representation (using next).
#[repr(C)]
pub struct PatternStep {
    pub event: u32, // Opaque for now, not doing any MIDI data processing
    pub delay: u16, // Delay to next item
    pub next: u16,  // Next step in loop
}
//const pattern_none: u16 = 0xFFFF;

#[repr(C)]
#[derive(PartialEq, PartialOrd, Eq, Clone, Copy, Debug)]
pub struct PatternAbs {
    pub event: u32,
    pub time: u16,
}
impl Ord for PatternAbs {
    fn cmp(&self, other: &Self) -> Ordering {
        Ord::cmp(&self.time, &other.time)
    }
}

// FIXME: Since this will eventually run on target, it might even be
// useful to modify in-place using the same representation as the C
// code.

// But let's first make some things that are actually useful, like
// beat rotation and merge.  For merge it is useful to have a software
// timer available as algorithm.  Maybe best to wrap the C code a bit
// more so Rust can call into it.

// It seems best to define everything as iterators instead of being
// bound to concrete representation as array slices.

/// Convert relative to absolute timing and erase next.
// pub fn pattern_rel_to_abs<'a, I: Iterator<Item = &'a PatternStep>> (psi: &'a I)
//   -> impl Iterator<Item = PatternAbs> + 'a
// {
//     let mut time: u16 = 0;
//     psi.map(
//         |step|
//         {
//             let new_step = PatternAbs {
//                 event: step.event,
//                 time: time
//             };
//             time += step.delay;
//             new_step
//         })
// }

// Make some examples with iteration patterns.  There are too many
// combination of move/borrow/mut borrow to keep track of.  Find
// something simple that just works.

// Do an in-place conversion from relative to absolute.  The delay
// field is used to represent the current time stamp instead of the
// distance to the next step.

pub fn pattern_make_abs(psi: &mut [PatternStep]) -> () {
    let mut time: u16 = 0;
    for ps in psi.iter_mut() {
        let delay = ps.delay;
        ps.delay = time;
        time += delay;
    }
}

const MAX_PATTERN_SIZE: usize = 64;

type VecPatternAbs = Vec<PatternAbs, MAX_PATTERN_SIZE>;
type HeapPatternAbs = BinaryHeap<PatternAbs, Min, MAX_PATTERN_SIZE>;

pub fn pattern_abs_from_step(ps: &[PatternStep]) -> VecPatternAbs {
    let mut time: u16 = 0;
    ps.iter()
        .map(|step| {
            let new_step = PatternAbs {
                event: step.event,
                time: time,
            };
            time += step.delay;
            new_step
        })
        .collect()
}

pub fn time_offset(abs: u16, offset: i16, len: u16) -> u16 {
    let iabs = abs as i16;
    let ilen = len as i16;
    let iadj = (((iabs + offset) % ilen) + ilen) % ilen;
    return iadj as u16;
}

pub fn pattern_abs_adjust(ps: &[PatternStep], offset: i16) {
    let mut pa = pattern_abs_from_step(ps);
    let len = pa.iter().fold(0, |acc, step| acc + step.time);
    for step in pa.iter_mut() {
        step.time = time_offset(step.time, offset, len);
    }
}
// Not sure if there is a no_std quicksort.  Just use the heap.
pub fn pattern_abs_sort(pa: &mut [PatternAbs]) {
    let mut heap: HeapPatternAbs = BinaryHeap::new();
    let len = pa.len();
    for i in 0..len {
        heap.push(pa[i]).unwrap();
    }
    for i in 0..len {
        pa[i] = heap.pop().unwrap();
    }
}

// For test.c
// Conventions used:
// - C size passes arrays as pointer + length
// - Rust size converts that using from_raw_parts_mut
#[no_mangle]
pub extern "C" fn pattern_test(ps_raw: *mut PatternStep, len: usize) {
    assert!(!ps_raw.is_null());
    let ps = unsafe { slice::from_raw_parts_mut(ps_raw, len) };
    if len >= 1 {
        ps[0].delay = 1;
    }
    if len >= 2 {
        ps[1].delay = 2;
    }
}

// #[no_mangle]
// pub extern "C" fn pattern_abs_c(ps_raw: *mut PatternStep, len: usize)  {
//     assert!(!ps_raw.is_null());
//     let ps_in = unsafe { slice::from_raw_parts_mut(ps_raw, len) };
//     let _ps_out = pattern_rel_to_abs(ps_in.iter());
// }

#[no_mangle]
pub extern "C" fn test_rotate(ps_raw: *mut PatternStep, len: usize) {
    assert!(!ps_raw.is_null());
    let ps = unsafe { slice::from_raw_parts_mut(ps_raw, len) };
    let _pa = pattern_abs_from_step(ps);
}

/// Main library initialization.
#[no_mangle]
pub extern "C" fn synth_tools_rs_init() {
    // FIXME: There is no log function.  Maybe make a small system
    // interface to provide logging?
    // println!("synth_tools_rs_init()");
}

/// Test. Something that can't be optimized away.
#[no_mangle]
pub extern "C" fn test_synth_tools_rs_add1(x: u32) -> u32 {
    x + 1
}
