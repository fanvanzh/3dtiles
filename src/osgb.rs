extern crate libc;
extern crate serde;
extern crate serde_json;
extern crate rayon;

use std::fs;
use std::io;

use osgb::rayon::prelude::*;

use std::path::Path;
use std::error::Error;

extern "C" {
    pub fn make_gltf(in_path: *const u8, out_path: *const u8) -> bool;

    fn osgb23dtile(name_in: *const u8, name_out: *const u8) -> bool;

    fn osgb23dtile_path(
        name_in: *const u8,
        name_out: *const u8,
        box_ptr: *mut f64,
        len: *mut i32,
        x: f64,
        y: f64,
        max_lvl: i32,
    ) -> *mut libc::c_void;

    fn osgb2glb(name_in: *const u8, name_out: *const u8) -> bool;

    fn transform_c(radian_x: f64, radian_y: f64, height_min: f64, ptr: *mut f64);

    pub fn epsg_convert(insrs: i32, val: *mut f64, gdal: *const i8) -> bool;

    fn degree2rad( val:f64 ) -> f64;

    fn meter_to_lati(m:f64) -> f64;

    fn meter_to_longti(m:f64, lati:f64) -> f64;
}

fn walk_path(dir: &Path, cb: &mut FnMut(&str)) -> io::Result<()> {
    if dir.is_dir() {
        for entry in fs::read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.is_dir() {
                walk_path(&path, cb)?;
            } else {
                if let Some(osdir) = path.extension() {
                    if osdir.to_str() == Some("osgb") {
                        cb(&path.to_str().unwrap());
                    }
                }
            }
        }
    }
    Ok(())
}

fn osgv_convert(dir_osgb: &str, dir_from: &str, dir_dest: &str) -> Result<(), Box<Error>> {
    unsafe {
        let mut source_vec = dir_osgb.to_string().as_bytes_mut().to_vec();
        source_vec.push(0x00);
        let dest_string = dir_osgb.clone().replace(".osgb", ".b3dm").replace(
            dir_from,
            dir_dest,
        );
        let mut dest_vec = dest_string.to_string().as_bytes_mut().to_vec();
        dest_vec.push(0x00);
        // create dir first
        let dest_path = Path::new(dest_string.as_str()).parent().unwrap();
        fs::create_dir_all(dest_path)?;
        if !osgb23dtile(source_vec.as_ptr(), dest_vec.as_ptr()) {
            return Err(From::from(format!("failed: {}", dir_osgb)));
        }
    }
    Ok(())
}

fn str_to_vec_c(str: &str) -> Vec<u8> {
    let mut buf = str.as_bytes().to_vec();
    buf.push(0x00);
    buf
}

#[derive(Debug)]
struct TileResult {
    json: String,
    path: String,
    box_v: Vec<f64>,
}

struct OsgbInfo {
    in_dir: String,
    out_dir: String,
    sender: ::std::sync::mpsc::Sender<TileResult>,
}

pub fn osgb_batch_convert(
    dir: &Path,
    dir_dest: &Path,
    max_lvl: Option<i32>,
    center_x: f64,
    center_y: f64,
    region_offset: Option<f64>,
) -> Result<(), Box<Error>> {
    use std::fs::File;
    use std::io::prelude::*;
    use std::thread;
    use std::sync::mpsc::channel;

    let path = dir.join("Data");
    // 指定 .\Data 目录
    if !path.exists() || !path.is_dir() {
        return Err(From::from(format!("dir {} not exist", path.display())));
    }

    let (sender, receiver) = channel();
    let mut osgb_dir_pair: Vec<OsgbInfo> = vec![];
    let mut task_count = 0;
    fs::create_dir_all(dir_dest)?;
    for entry in fs::read_dir(&path)? {
        let entry = entry?;
        let path_tile = entry.path();
        if path_tile.is_dir() {
            // if Tile_xx_xx.osgb
            let stem = path_tile.file_stem().unwrap().to_str().unwrap();
            let osgb = path_tile.join(stem).with_extension("osgb");
            if osgb.exists() && !osgb.is_dir() {
                // convert this path
                task_count += 1;
                //let in_buf = str_to_vec_c(osgb.to_str().unwrap());
                let out_dir = dir_dest.join("Data").join(stem);
                fs::create_dir_all(&out_dir)?;
                osgb_dir_pair.push(OsgbInfo{
                    in_dir: osgb.to_string_lossy().into(),
                    out_dir: out_dir.to_string_lossy().into(),
                    sender: sender.clone()
                });
            } else {
                error!("dir error: {}", osgb.display());
            }
        }
    }
    
    let rad_x = unsafe {
         degree2rad(center_x)
    };
    let rad_y = unsafe {
        degree2rad(center_y)
    };

    let max_lvl: i32 = max_lvl.unwrap_or(100);
    osgb_dir_pair.into_par_iter().map( | info | {
        unsafe {
	        let mut root_box = vec![0f64; 6];
	        let mut json_buf = vec![];
	        let mut json_len = 0i32;
	        let in_ptr = str_to_vec_c(&info.in_dir);
	        let out_ptr = str_to_vec_c(&info.out_dir);
	        let out_ptr = osgb23dtile_path(
	            in_ptr.as_ptr(),
	            out_ptr.as_ptr(),
	            root_box.as_mut_ptr(),
	            (&mut json_len) as *mut i32,
                rad_x,rad_y,max_lvl
	        );
	        if out_ptr.is_null() {
	            error!("failed: {}", info.in_dir);
	        } else {
	            json_buf.resize(json_len as usize, 0);
	            libc::memcpy(
	                json_buf.as_mut_ptr() as *mut libc::c_void,
	                out_ptr,
	                json_len as usize,
	            );
	            libc::free(out_ptr);
	        }
	        let t = TileResult {
                path: info.out_dir.into(),
	            json: String::from_utf8(json_buf).unwrap(),
	            box_v: root_box,
	        };
	        info.sender.send(t).unwrap();
	    }
    }).count();

    // merge and root
    let mut tile_array = vec![];
    for _ in 0..task_count {
        if let Ok(t) = receiver.recv() {
            if !t.json.is_empty() {
                tile_array.push(t);
            }
        }
    }
    let mut root_box = vec![-1.0E+38f64, -1.0E+38, -1.0E+38, 1.0E+38, 1.0E+38, 1.0E+38];
    for x in tile_array.iter() {
        for i in 0..3 {
            if x.box_v[i] > root_box[i] {
                root_box[i] = x.box_v[i]
            }
        }
        for i in 3..6 {
            if x.box_v[i] < root_box[i] {
                root_box[i] = x.box_v[i]
            }
        }
    }

    //let root_geometric_error = get_geometric_error(center_y, 10);
    // do merge plz
    let mut tras_height = 0f64;
    if let Some(v) = region_offset {
        tras_height = v - root_box[5];
    }
    let mut trans_vec = vec![0f64; 16];
    unsafe {
        transform_c(center_x, center_y, tras_height, trans_vec.as_mut_ptr());
    }
    let mut root_json = json!(
        {
            "asset": {
                "version": "1.0",
                "gltfUpAxis": "Z"  
            }, 
            "geometricError": 2000,
            "root" : {
                "transform" : trans_vec,
                "boundingVolume" : {
                    "box": box_to_tileset_box(&root_box)
                },
                "geometricError": 2000,
                "children": []
            }
        }
    );

    let out_dir: String = dir_dest.to_string_lossy().into();
    for x in tile_array {
        let mut path = x.path;
        let mut json_val : serde_json::Value = serde_json::from_str(&x.json).unwrap();
        let mut tile_box = json_val["boundingVolume"]["box"].as_array().unwrap();
        let tile_object = json!(
            {
                "boundingVolume": {
                    "box": &tile_box
                },
                "geometricError": 1000,
                "content": {
                    "url" : format!("{}/tileset.json", path.replace(&out_dir,".").replace("\\","/"))
                }
            }
        );
        root_json["root"]["children"].as_array_mut().unwrap().push(tile_object);
        let sub_tile = json!({
            "asset": {
                "version": "1.0",
                "gltfUpAxis":"Z"
            },
            "root": json_val
        }
        );
        let out_file = path.clone() + "/tileset.json";
        let mut f = File::create(out_file)?;
        f.write_all(serde_json::to_string_pretty(&sub_tile).unwrap().as_bytes())?;
    }
    let path_json = dir_dest.join("tileset.json");
    let mut f = File::create(path_json)?;
    f.write_all(serde_json::to_string_pretty(&root_json).unwrap().as_bytes())?;
    Ok(())
}

fn get_geometric_error(center_y: f64, lvl: i32) -> f64 {
    use std::f64;
    let x = center_y * f64::consts::PI / 180.0;
    let round = x.cos() * 2.0 * f64::consts::PI * 6378137.0;
    let pow = 2i32.pow(lvl as u32 - 2);
    4.0 * round / (256 * pow) as f64
}

fn box_to_tileset_box(box_v: &Vec<f64>) -> Vec<f64> {
    let mut box_new = vec![];
    box_new.push((box_v[0] + box_v[3]) / 2.0);
    box_new.push((box_v[1] + box_v[4]) / 2.0);
    box_new.push((box_v[2] + box_v[5]) / 2.0);

    box_new.push((box_v[3] - box_v[0]) / 2.0);
    box_new.push(0.0);
    box_new.push(0.0);

    box_new.push(0.0);
    box_new.push((box_v[4] - box_v[1]) / 2.0);
    box_new.push(0.0);

    box_new.push(0.0);
    box_new.push(0.0);
    box_new.push((box_v[5] - box_v[2]) / 2.0);

    box_new
}
