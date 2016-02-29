#![no_std]
#![feature(asm)]
#[cfg(target_arch = "arm")]
pub mod atomics;
