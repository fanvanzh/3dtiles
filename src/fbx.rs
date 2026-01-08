use std::{error::Error, fs};

use crate::common::str_to_vec_c;

extern "C" {

    fn fbx23dtile(
        in_path: *const u8,
        out_path: *const u8,
        box_ptr: *mut f64,
        len: *mut i32,
        max_lvl: i32,
        enable_texture_compress: bool,
        enable_meshopt: bool,
        enable_draco: bool,
        enable_unlit: bool,
        longitude: f64,
        latitude: f64,
        height: f64,
    ) -> *mut libc::c_void;
}

pub fn convert_fbx(
    in_file: &str,
    out_dir: &str,
    max_lvl: Option<i32>,
    enable_texture_compress: bool,
    enable_meshopt: bool,
    enable_draco: bool,
    enable_unlit: bool,
    longitude: f64,
    latitude: f64,
    height: f64,
) -> Result<(), Box<dyn Error>> {
    let in_path = str_to_vec_c(in_file);
    let out_path = str_to_vec_c(out_dir);

    // Create output directory
    fs::create_dir_all(out_dir)?;

    let max_lvl = max_lvl.unwrap_or(5);
    let mut root_box = vec![0f64; 6];
    let mut json_len = 0i32;

    unsafe {
        let out_ptr = fbx23dtile(
            in_path.as_ptr(),
            out_path.as_ptr(),
            root_box.as_mut_ptr(),
            &mut json_len,
            max_lvl,
            enable_texture_compress,
            enable_meshopt,
            enable_draco,
            enable_unlit,
            longitude,
            latitude,
            height,
        );

        if out_ptr.is_null() {
            return Err(From::from(format!("FBX conversion failed for {}", in_file)));
        }
        libc::free(out_ptr);
    }

    Ok(())
}
