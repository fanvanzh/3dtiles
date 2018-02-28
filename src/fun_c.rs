#[no_mangle]
pub extern fn write_file(file_name: *const i8, buf: *const u8, buf_len: u32) -> bool {
	use std::ffi;
	use std::slice;
	use std::fs::File;
	use std::io::prelude::*;

	unsafe{
		let file_name = ffi::CStr::from_ptr(file_name).to_str().unwrap();
		if let Ok(mut f) = File::create(file_name) {
			let arr = slice::from_raw_parts(buf,buf_len as usize);
			match f.write_all(arr) {
				Ok(_) => { true }  
				Err(e) => {
					println!("ERROR: {:?}", e); 
					false 
				} 
			}
		}
		else {
			false
		}	
	}
}

#[no_mangle]
pub extern fn mkdirs(path : *const i8) -> bool {
	use std::fs;
	use std::ffi;
	unsafe {
		match ffi::CStr::from_ptr(path).to_str() {
			Ok(buf) => {
				match fs::create_dir_all(buf) {
					Ok(_) => { true } 
					Err(e) => { println!("ERROR: {:?}", e); false }
				}	
			}
			Err(e) =>{
				println!("ERROR: {:?}", e);
				false
			}
		}
	}
}
