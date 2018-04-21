extern "C" {
    fn shp23dtile(name: *const u8, layer: i32, 
        dest: *const u8, height: *const u8) -> bool;
}

extern crate serde;
extern crate serde_json;

use std::fs;
use std::io;
use std::fs::File;
use std::path::Path;
use std::error::Error;
use std::io::prelude::*;

fn walk_path(dir: &Path, cb: &mut FnMut(&str)) -> io::Result<()> {
    if dir.is_dir() {
        for entry in fs::read_dir(dir)? {
            let entry = entry?;
            let path = entry.path();
            if path.is_dir() {
                walk_path(&path, cb)?;
            } else {
                if let Some(osdir) = path.extension() {
                    if osdir.to_str() == Some("json") {
                        cb(&path.to_str().unwrap());
                    }
                }
            }
        }
    }
    Ok(())
}


pub fn shape_batch_convert(from: &str, to: &str, height: &str) -> bool{
    unsafe {
        let mut source_vec = String::from(from);
        source_vec.push('\0');
        let mut dest_vec = String::from(to);
        dest_vec.push('\0');
        let mut height_vec = String::from(height);
        height_vec.push('\0');
        let res = shp23dtile(source_vec.as_ptr(), 0, dest_vec.as_ptr(), height_vec.as_ptr());
        if !res { return res; }
        // meger the tile 
        // minx,miny,maxx,maxy
        let mut root_tile = String::from(r#"{"asset": {"version": "0.0",
            "gltfUpAxis": "Y"},
            "geometricError":200,
            "root": {
            "#);
        let mut root_region = vec![1.0E+38f64, 1.0E+38, -1.0E+38, -1.0E+38, 1.0E+38, -1.0E+38];
        let mut json_vec = vec![];
        walk_path(&Path::new(to).join("tile"),&mut |dir| {
            let file = File::open(dir).unwrap();
            let val : serde_json::Value = serde_json::from_reader(file).unwrap();
            let region = val["root"]["boundingVolume"]["region"].as_array().unwrap();
            for &x in [0,1,4].iter() {
                let val = region[x].as_f64().unwrap();
                if val < root_region[x] {
                    root_region[x] = val;
                }    
            }
            for &x in [2,3,5].into_iter() {
                let val = region[x].as_f64().unwrap();
                if val > root_region[x] {
                    root_region[x] = val;
                }    
            }
            let relative = dir.replace(to,".").replace("\\","/");
            json_vec.push(relative);
        });
        root_tile += &format!(r#""boundingVolume": {{"region":[{},{},{},{},{},{}]}},"children":["#,
                root_region[0],
                root_region[1],
                root_region[2],
                root_region[3],
                root_region[4],
                root_region[5]
            );
        for x in json_vec.iter() {
            root_tile += &format!(r#""{}","#,x);
        }
        root_tile.pop();
        root_tile += "]}}";
        let dir_dest = Path::new(to);
        let path_json = dir_dest.join("tileset.json");
        let mut f = File::create(path_json).unwrap();
        f.write_all(root_tile.as_bytes()).unwrap();
        true
    }
}
