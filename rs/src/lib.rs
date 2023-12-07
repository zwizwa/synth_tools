
#![no_std] // don't link the Rust standard library
extern crate panic_halt;
extern crate heapless;

// In no_std setup we need to use this instead of std::slice
// https://doc.rust-lang.org/core/slice/fn.from_raw_parts_mut.html
use core::slice;
use heapless::Vec;

// Assume that u32 is C uint32_t etc... Is that ok?

// The same structure is used for a linear sequence of events, and a
// circular representation (using next).
#[repr(C)]
pub struct pattern_step {
    pub event: u32, // Opaque for now, not doing any MIDI data processing
    pub delay: u16, // Delay to next item
    pub next: u16, // Next step in loop
}
//const pattern_none: u16 = 0xFFFF;

#[repr(C)]
pub struct pattern_abs {
    pub event: u32,
    pub time: u16,
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
// pub fn pattern_rel_to_abs<'a, I: Iterator<Item = &'a pattern_step>> (psi: &'a I)
//   -> impl Iterator<Item = pattern_abs> + 'a
// {
//     let mut time: u16 = 0;
//     psi.map(
//         |step|
//         {
//             let new_step = pattern_abs {
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

pub fn pattern_make_abs(psi: &mut [pattern_step]) -> () {
    let mut time: u16 = 0;
    for ps in psi.iter_mut() {
        let delay = ps.delay;
        ps.delay = time;
        time += delay;
    }
}

const MAX_PATTERN_SIZE: usize = 64;

pub fn test_heapless_vec(ps: &[pattern_step]) -> u16 {
    let mut time: u16 = 0;
    let pa: Vec<pattern_abs, MAX_PATTERN_SIZE> =
        ps.iter().map(
            |step| {
                let new_step = pattern_abs {
                    event: step.event,
                    time: time
                };
                time += step.delay;
                new_step
            }).collect();
    pa.iter().fold(
        0,
        |acc, step| {
            acc + step.time
        })
}


// FIXME: Use heapless Vec
//pub fn pattern_make_abs_copy(psi: &[pattern_step]) -> () {
//    let mut time: u16 = 0;
//    let mut out: [pattern_abs; psi.len];
//}


// For test.c
// Conventions used:
// - C size passes arrays as pointer + length
// - Rust size converts that using from_raw_parts_mut
#[no_mangle]
pub extern "C" fn pattern_test(ps_raw: *mut pattern_step, len: usize)  {
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
// pub extern "C" fn pattern_abs_c(ps_raw: *mut pattern_step, len: usize)  {
//     assert!(!ps_raw.is_null());
//     let ps_in = unsafe { slice::from_raw_parts_mut(ps_raw, len) };
//     let _ps_out = pattern_rel_to_abs(ps_in.iter());
// }
