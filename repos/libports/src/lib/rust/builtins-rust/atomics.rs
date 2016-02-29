macro_rules! sync_val_compare_and_swap {
    ($ptr:expr, $oldval:expr, $newval:expr) => {{
        unsafe {
        asm!("dmb");
        let tmp = *$ptr;
        asm!("dmb");
        if tmp == $oldval {
            asm!("dmb");
            *$ptr = $newval;
            asm!("dmb");
        }
        tmp
        }
    }}
}

#[no_mangle]
pub extern fn __sync_val_compare_and_swap_1(ptr: *mut u8,oldval: u8,newval: u8) -> u8 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_2(ptr: *mut u16,oldval: u16,newval: u16) -> u16 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_4(ptr: *mut u32,oldval: u32,newval: u32) -> u32 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
#[no_mangle]
pub extern fn __sync_val_compare_and_swap_8(ptr: *mut u64,oldval: u64,newval: u64) -> u64 {
    sync_val_compare_and_swap!(ptr,oldval,newval)
}
