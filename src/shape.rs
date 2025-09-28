extern "C" {
    fn shp23dtile(name: *const u8, layer: i32, dest: *const u8, height: *const u8) -> bool;
}

use std::ffi::CString;
use std::fs;
use std::fs::File;
use std::io;
use std::io::prelude::*;
use std::path::Path;

use serde_json;

fn walk_path(dir: &Path, cb: &mut dyn FnMut(&str)) -> io::Result<()> {
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

pub fn shape_batch_convert(from: &str, to: &str, height: &str) -> bool {
    unsafe {
        let source_vec = CString::new(from).unwrap();
        let dest_vec = CString::new(to).unwrap();
        let height_vec = CString::new(height).unwrap();
        let res = shp23dtile(
            source_vec.as_ptr() as *const u8,
            0,
            dest_vec.as_ptr() as *const u8,
            height_vec.as_ptr() as *const u8,
        );
        if !res {
            return res;
        }
        // meger the tile
        // minx,miny,maxx,maxy
        let mut tileset_json = json!({
            "asset":{
                "version":"0.0",
                "gltfUpAxis":"Z"
            },
            "geometricError":200,
            "root":{
                "refine":"REPLACE",
                "boundingVolume":{
                    "region":[]
                },
                "geometricError":200,
                "children":[]
            }
        });
        let mut root_region = vec![1.0E+38f64, 1.0E+38, -1.0E+38, -1.0E+38, 1.0E+38, -1.0E+38];
        let mut json_vec = vec![];
        walk_path(&Path::new(to).join("tile"), &mut |dir| {
            let file = File::open(dir).unwrap();
            let val: serde_json::Value = serde_json::from_reader(file).unwrap();
            let region = val["root"]["boundingVolume"]["region"].as_array().unwrap();
            for &x in [0, 1, 4].iter() {
                let val = region[x].as_f64().unwrap();
                if val < root_region[x] {
                    root_region[x] = val;
                }
            }
            for &x in [2, 3, 5].iter() {
                let val = region[x].as_f64().unwrap();
                if val > root_region[x] {
                    root_region[x] = val;
                }
            }
            json_vec.push(val["root"].clone());
        })
        .expect("walk_path failed!");

        {
            let region = tileset_json["root"]["boundingVolume"]["region"]
                .as_array_mut()
                .unwrap();
            for x in root_region {
                region.push(json!(x));
            }
        }
        {
            let children = tileset_json["root"]["children"].as_array_mut().unwrap();
            for x in json_vec {
                children.push(json!(x));
            }
        }

        let dir_dest = Path::new(to);
        let path_json = dir_dest.join("tileset.json");
        let mut f = File::create(path_json).unwrap();
        f.write_all(
            &serde_json::to_string_pretty(&tileset_json)
                .unwrap()
                .into_bytes(),
        )
        .unwrap();
        true
    }
}
