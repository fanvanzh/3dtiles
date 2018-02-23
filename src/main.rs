extern crate libc;
extern crate encoding;

use std::time;

extern "C" {
	fn shp23dtile(name: *const u8, layer: i32, dest: *const u8) -> bool;

	//fn test() -> bool;
}

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

////////////////////////////
use std::io;
use std::ffi;
use std::path::Path;
use std::fs::{self, DirEntry};

#[derive(Debug)]
pub struct TileInfo {
	name : String,
	url : String,
    children : Vec<TileInfo>
}

pub fn visit_path(dir: &Path) -> Result<TileInfo,io::Error> {
	let mut tl = TileInfo{
			name: "xx".into(),
			url: dir.to_str().unwrap().into(),
			children: vec![]
	};
	if dir.is_dir() {
		for entry in fs::read_dir(dir)? {
			let entry = entry?;
            let path = entry.path();
            if !path.is_dir() {
            	if path.extension().unwrap().to_str() != Some("json") {
            		continue;
            	}
            }
        	let res = visit_path(&path);
        	if let Ok(tile) = res {
        		tl.children.push(tile);
        	}
		}
	}
	else {
		// if name is *.json, parse json and merge
		// tl.url = dir.to_str().unwrap().into();
	}
	Ok(tl)
}

#[no_mangle]
pub extern fn merge_tileset(path: *const i8) -> bool{

	unsafe {
		match ffi::CStr::from_ptr(path).to_str() {
			Ok(buf) => {
				//  Walk -> Struct -> Json
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

fn main() {
	use std::io;
	let mut msg = String::new();
	io::stdin().read_line(&mut msg).unwrap();
	let shpfile = r#"E:\test\buildings\osm_bd_height_rd.shp"#;
	let dest = r#"E:\test\buildings\结果文件"#;

	let res_tile = visit_path(Path::new(dest));
	println!("{:#?}", res_tile.unwrap());
	//let mut rs = GB18030.encode(shpfile, EncoderTrap::Strict).unwrap();
	unsafe {
		let mut source_vec = shpfile.to_string()
			.as_bytes_mut().to_vec();
		source_vec.push(0x00);

		let mut dest_vec = dest.to_string()
			.as_bytes_mut().to_vec();
		dest_vec.push(0x00);

		let tick = time::SystemTime::now();
		shp23dtile(source_vec.as_ptr(), 0, dest_vec.as_ptr());
		println!("{:?}", tick.elapsed());
	}
}