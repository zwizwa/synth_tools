
#![no_std] // don't link the Rust standard library
extern crate panic_halt;

// In no_std setup we need to use this instead of std::slice
// https://doc.rust-lang.org/core/slice/fn.from_raw_parts_mut.html
use core::slice;

// Assume that u32 is C uint32_t etc... Is that ok?
#[repr(C)]
pub struct pattern_step {
    pub event: u32, // Opaque for now, not doing any MIDI data processing
    pub delay: u16, // Delay to next item
    pub next: u16, // Next step in loop
}

// FIXME: Since this will eventually run on target, it might even be
// useful to modify in-place using the same representation as the C
// code.

// But let's first make some things that are actually useful, like
// beat rotation and merge.  For merge it is useful to have a software
// timer available as algorithm.  Maybe best to wrap the C code a bit
// more so Rust can call into it.


// For test.c
// Conventions used:
// - C size passes arrays as pointer + length
// - Rust size converts that using from_raw_parts_mut
#[no_mangle]
pub extern "C" fn pattern_rotate(ps_raw: *mut pattern_step, len: usize)  {
    assert!(!ps_raw.is_null());
    let ps = unsafe { slice::from_raw_parts_mut(ps_raw, len) };
    if len >= 1 {
        ps[0].delay = 1;
    }
    if len >= 2 {
        ps[1].delay = 2;
    }
}

