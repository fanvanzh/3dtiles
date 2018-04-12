extern crate libc;
extern crate serde;
extern crate serde_json;

use std::fs;
use std::io;
use std::path::Path;
use std::error::Error;

extern "C" {
    fn osgb23dtile(name_in: *const u8, name_out: *const u8) -> bool;

    fn osgb23dtile_path(
        name_in: *const u8,
        name_out: *const u8,
        box_ptr: *mut f64,
        len: *mut i32,
        max_lvl: i32,
    ) -> *mut libc::c_void;

    fn osgb2glb(name_in: *const u8, name_out: *const u8) -> bool;

    fn transform_c(radian_x: f64, radian_y: f64, height_min: f64, ptr: *mut f64);
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
    box_v: Vec<f64>,
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
                let in_buf = str_to_vec_c(osgb.to_str().unwrap());
                let out_dir = dir_dest.join("Data").join(stem);
                fs::create_dir_all(&out_dir)?;
                let out_buf = str_to_vec_c(out_dir.to_str().unwrap());
                let path_clone = path_tile.clone();
                let sender_clone = sender.clone();
                thread::spawn(move || unsafe {
                    let mut root_box = vec![0f64; 6];
                    let mut json_buf = vec![];
                    let mut json_len = 0i32;
                    let out_ptr = osgb23dtile_path(
                        in_buf.as_ptr(),
                        out_buf.as_ptr(),
                        root_box.as_mut_ptr(),
                        (&mut json_len) as *mut i32,
                        max_lvl.unwrap_or(100),
                    );
                    if out_ptr.is_null() {
                        error!("failed: {}", path_clone.display());
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
                        json: String::from_utf8(json_buf).unwrap(),
                        box_v: root_box,
                    };
                    sender_clone.send(t).unwrap();
                });

            } else {
                error!("dir error: {}", osgb.display());
            }
        }
    }

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

    let root_geometric_error = get_geometric_error(center_y, 10);
    let mut tileset_json = String::new();
    tileset_json +=
        r#"{"asset": {    "version": "0.0",    "gltfUpAxis": "Y"  },  "geometricError":"#;
    tileset_json += root_geometric_error.to_string().as_str();
    tileset_json += r#","root": "#;

    // do merge plz
    let mut tras_height = 0f64;
    if let Some(v) = region_offset {
        tras_height = v - root_box[5];
    }
    let mut trans_vec = vec![0f64; 16];
    unsafe {
        transform_c(center_x, center_y, tras_height, trans_vec.as_mut_ptr());
    }
    //
    let root_json = RootJson {
        refine: "REPLACE".into(),
        transform: trans_vec,
        boundingVolume: Volume { box_v: box_to_tileset_box(&root_box) },
        //content: Content { url: "".into() },
        geometricError: get_geometric_error(center_y, 10),
        children: vec![],
    };
    let json_str = serde_json::to_string(&root_json).unwrap();
    tileset_json += &json_str;
    tileset_json.pop(); // }
    tileset_json.pop(); // ]
    for x in tile_array {
        tileset_json += &x.json;
        tileset_json += ",";
    }
    tileset_json.pop();
    tileset_json.push(']');
    tileset_json.push('}'); // end-root
    tileset_json.push('}'); // end-json
    let path_json = dir_dest.join("tileset.json");
    let mut f = File::create(path_json)?;
    f.write_all(tileset_json.as_bytes())?;
    Ok(())
}

#[derive(Serialize, Clone, Default)]
struct Volume {
    #[serde(rename = "box")]
    box_v: Vec<f64>,
}

#[derive(Serialize, Clone, Default)]
struct Content {
    boundingVolume: Volume,
    url: String,
}

#[derive(Serialize)]
struct TileJson {
    boundingVolume: Volume,
    content: Content,
    geometricError: f64,
    children: Vec<TileJson>,
}

#[derive(Serialize)]
struct RootJson {
    refine: String,
    transform: Vec<f64>,
    boundingVolume: Volume,
    geometricError: f64,
    children: Vec<TileJson>,
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
