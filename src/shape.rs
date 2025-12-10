extern "C" {
    fn shp23dtile(
        name: *const u8,
        layer: i32,
        dest: *const u8,
        height: *const u8,
        enable_simplify: bool) -> bool;
}

use std::ffi::CString;
use std::fs;
use std::fs::File;
use std::io;
use std::io::prelude::*;
use std::path::Path;

use serde_json;
use std::f64::{INFINITY, NEG_INFINITY};

// Compute geometricError from 3D corners (use half of max span)
fn compute_geometric_error_from_corners(corners: &[[f64; 3]]) -> f64 {
    if corners.is_empty() {
        return 0.0;
    }
    let mut min_v = [INFINITY; 3];
    let mut max_v = [NEG_INFINITY; 3];
    for c in corners {
        for i in 0..3 {
            if c[i] < min_v[i] {
                min_v[i] = c[i];
            }
            if c[i] > max_v[i] {
                max_v[i] = c[i];
            }
        }
    }
    let span = [max_v[0] - min_v[0], max_v[1] - min_v[1], max_v[2] - min_v[2]];
    let max_span = span
        .iter()
        .fold(0.0_f64, |acc, v| acc.max(*v));
    max_span * 0.5
}

fn corners_from_box(box_vals: &[f64]) -> Vec<[f64; 3]> {
    let mut corners = Vec::with_capacity(8);
    if box_vals.len() < 12 {
        return corners;
    }
    let c = [box_vals[0], box_vals[1], box_vals[2]];
    let a1 = [box_vals[3], box_vals[4], box_vals[5]];
    let a2 = [box_vals[6], box_vals[7], box_vals[8]];
    let a3 = [box_vals[9], box_vals[10], box_vals[11]];
    for sx in [-1.0_f64, 1.0] {
        for sy in [-1.0_f64, 1.0] {
            for sz in [-1.0_f64, 1.0] {
                corners.push([
                    c[0] + sx * a1[0] + sy * a2[0] + sz * a3[0],
                    c[1] + sx * a1[1] + sy * a2[1] + sz * a3[1],
                    c[2] + sx * a1[2] + sy * a2[2] + sz * a3[2],
                ]);
            }
        }
    }
    corners
}

// Apply column-major 4x4 transform to point
fn transform_point(m: &[f64], p: &[f64; 3]) -> [f64; 3] {
    if m.len() < 16 {
        return *p;
    }
    let x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    let y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    let z = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
    [x, y, z]
}

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

pub fn shape_batch_convert(from: &str, to: &str, height: &str, enable_simplify: bool) -> bool {
    unsafe {
        let source_vec = CString::new(from).unwrap();
        let dest_vec = CString::new(to).unwrap();
        let height_vec = CString::new(height).unwrap();
        let res = shp23dtile(
            source_vec.as_ptr() as *const u8,
            0,
            dest_vec.as_ptr() as *const u8,
            height_vec.as_ptr() as *const u8,
            enable_simplify);
        if !res {
            return res;
        }
        // If the native generator already emitted a hierarchical root tileset, do not
        // re-merge everything into a flat root here. This avoids clobbering the
        // parent/child tileset structure produced in C++.
        if Path::new(to).join("tileset.json").exists() {
            return true;
        }
        // meger the tile
        // minx,miny,maxx,maxy
        let mut tileset_json = json!({
            "asset":{
                "version":"0.0",
                "gltfUpAxis":"Z"
            },
            "geometricError":0,
            "root":{
                "refine":"REPLACE",
                "boundingVolume":{
                    "box":[]
                },
                "geometricError":0,
                "children":[]
            }
        });
        let mut root_min = [INFINITY, INFINITY, INFINITY];
        let mut root_max = [NEG_INFINITY, NEG_INFINITY, NEG_INFINITY];
        let mut json_vec = vec![];
        walk_path(&Path::new(to).join("tile"), &mut |dir| {
            let file = File::open(dir).unwrap();
            let val: serde_json::Value = serde_json::from_reader(file).unwrap();
            let bv = &val["root"]["boundingVolume"];
            let box_arr = match bv["box"].as_array() {
                Some(arr) => arr,
                None => return,
            };
            if box_arr.len() < 12 {
                return;
            }
            let box_vals: Vec<f64> = box_arr
                .iter()
                .map(|v| v.as_f64().unwrap_or(0.0))
                .collect();
            let mut corners = corners_from_box(&box_vals);

            // Apply transform if present (column-major 4x4) to get world-space corners
            if let Some(trans_arr) = val["root"]["transform"].as_array() {
                if trans_arr.len() == 16 {
                    let m: Vec<f64> = trans_arr
                        .iter()
                        .map(|v| v.as_f64().unwrap_or(0.0))
                        .collect();
                    for c in corners.iter_mut() {
                        *c = transform_point(&m, c);
                    }
                }
            }

            let ge = compute_geometric_error_from_corners(&corners);

            for c in corners {
                for i in 0..3 {
                    if c[i] < root_min[i] {
                        root_min[i] = c[i];
                    }
                    if c[i] > root_max[i] {
                        root_max[i] = c[i];
                    }
                }
            }

            let mut child = val["root"].clone();
            child["geometricError"] = json!(ge);
            json_vec.push(child);
        })
        .expect("walk_path failed!");

        let has_children = root_min[0].is_finite() && root_max[0].is_finite();
        let (root_box, root_ge) = if has_children {
            let dx = root_max[0] - root_min[0];
            let dy = root_max[1] - root_min[1];
            let dz = root_max[2] - root_min[2];
            let root_center = [root_min[0] + dx * 0.5, root_min[1] + dy * 0.5, root_min[2] + dz * 0.5];
            let box_vals = [
                root_center[0], root_center[1], root_center[2],
                dx * 0.5, 0.0, 0.0,
                0.0, dy * 0.5, 0.0,
                0.0, 0.0, dz * 0.5,
            ];
            let corners = corners_from_box(&box_vals);
            let ge = compute_geometric_error_from_corners(&corners);
            (box_vals, ge)
        } else {
            ([0.0_f64; 12], 0.0_f64)
        };
        {
            let box_arr = tileset_json["root"]["boundingVolume"]["box"]
                .as_array_mut()
                .unwrap();
            for x in root_box {
                box_arr.push(json!(x));
            }
        }
        {
            tileset_json["geometricError"] = json!(root_ge);
            tileset_json["root"]["geometricError"] = json!(root_ge);
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
