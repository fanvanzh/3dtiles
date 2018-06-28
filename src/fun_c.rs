
#[no_mangle]
pub extern "C" fn write_file(file_name: *const i8, buf: *const u8, buf_len: u32) -> bool {
    use std::ffi;
    use std::slice;
    use std::fs::File;
    use std::error::Error;
    use std::io::prelude::*;

    unsafe {
        if let Ok(file_name) = ffi::CStr::from_ptr(file_name).to_str() {
            if let Ok(mut f) = File::create(file_name) {
                let arr = slice::from_raw_parts(buf, buf_len as usize);
                match f.write_all(arr) {
                    Ok(_) => true,
                    Err(e) => {
                        error!("{}", e.description());
                        false
                    }
                }
            } else {
                false
            }
        } else {
            error!("convert file_name fail");
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn mkdirs(path: *const i8) -> bool {
    use std::fs;
    use std::ffi;
    use std::error::Error;
    unsafe {
        match ffi::CStr::from_ptr(path).to_str() {
            Ok(buf) => {
                match fs::create_dir_all(buf) {
                    Ok(_) => true,
                    Err(e) => {
                        error!("{}", e.description());
                        false
                    }
                }
            }
            Err(e) => {
                error!("{}", e.description());
                false
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn log_error(msg: *const i8) {
    use std::ffi::CStr;
    unsafe {
        let input = CStr::from_ptr(msg);
        error!("{}", input.to_string_lossy()); 
    }
}