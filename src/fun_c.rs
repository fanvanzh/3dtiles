#[no_mangle]
pub extern "C" fn write_file(file_name: *const libc::c_char, buf: *const u8, buf_len: u32) -> bool {
    use std::ffi;
    use std::fs;
    use std::fs::File;
    use std::io::prelude::*;
    use std::slice;

    unsafe {
        if let Ok(file_name) = ffi::CStr::from_ptr(file_name).to_str() {
            use std::path::Path;
            let path = Path::new(file_name);
            if let Some(parent) = path.parent() {
                if !parent.as_os_str().is_empty() {
                    if let Err(e) = fs::create_dir_all(parent) {
                        error!("create dir fail: {}", e);
                        return false;
                    }
                }
            }
            if let Ok(mut f) = File::create(file_name) {
                let arr = slice::from_raw_parts(buf, buf_len as usize);
                match f.write_all(arr) {
                    Ok(_) => true,
                    Err(e) => {
                        error!("{}", e);
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
pub extern "C" fn mkdirs(path: *const libc::c_char) -> bool {
    use std::ffi;
    use std::fs;
    unsafe {
        match ffi::CStr::from_ptr(path).to_str() {
            Ok(buf) => match fs::create_dir_all(buf) {
                Ok(_) => true,
                Err(e) => {
                    error!("{}", e);
                    false
                }
            },
            Err(e) => {
                error!("{}", e);
                false
            }
        }
    }
}
