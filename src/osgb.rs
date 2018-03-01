
use std::fs;
use std::io;
use std::path::Path;
use std::error::Error;

use rayon::prelude::*;
use serde_json;

extern "C" {
	fn osgb23dtile(
		name_in: *const u8 , name_out: *const u8, 
		center_x: f64, center_y: f64 ) -> bool;

	fn osgb2glb(name_in: *const u8 , name_out: *const u8 ) -> bool;
	
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

fn osgv_convert(
	dir_osgb: &str, dir_from: &str, 
	dir_dest: &str, center_x: f64, center_y: f64) -> Result<(), Box<Error>> {
	unsafe {
			let mut source_vec = dir_osgb.to_string()
			.as_bytes_mut().to_vec();
			source_vec.push(0x00);
			let dest_string = dir_osgb.clone()
				.replace(".osgb",".b3dm")
				.replace(dir_from, dir_dest);
			//println!("{:?} -- > {:?}", dir_osgb, dest_string);
			let mut dest_vec = dest_string.to_string()
				.as_bytes_mut().to_vec();
			dest_vec.push(0x00);
			// create dir first
			let dest_path = Path::new(dest_string.as_str()).parent().unwrap();
			fs::create_dir_all(dest_path)?;
			if !osgb23dtile(
				source_vec.as_ptr(), dest_vec.as_ptr(),
				center_x, center_y
				) {
				return Err(From::from(format!("failed: {}", dir_osgb)));
			}
	}
	Ok(())
}

pub fn osgb_batch_convert(
	dir: &Path, dir_dest:&Path, 
	center_x: f64, center_y: f64,
	max_lvl: Option<i32>
	) -> Result<(),Box<Error>>{
	
	let path = dir.join("Data");
	// 指定 .\Data 目录
	if !path.exists() || !path.is_dir() { return Err(From::from("dir Data not exist")); }

	fs::create_dir_all(dir_dest)?;
	let mut osg_array = vec![];
	{
		walk_path(
			dir, &mut |dir_osgb:&str| {
			if let Some(max_lvl) = max_lvl {
				let lvl_str = find_lvl_num(dir_osgb);
				if lvl_str.len() > 0 {
					let lvl_num: i32 = lvl_str.parse().unwrap();
					if lvl_num <= max_lvl {
						osg_array.push(dir_osgb.to_string());
					}
				}
			}
			else {
				osg_array.push(dir_osgb.to_string());
			}
		})?;
	}
	osg_array.par_iter().map(|x| {
		osgv_convert(
			x.as_str(), 
			dir.to_str().unwrap(), 
			dir_dest.to_str().unwrap(),
			center_x, center_y);
	}).count();
	Ok(())
}


use std::rc::Rc;
use std::cell::{RefCell};

#[derive(Serialize,Clone,Default)]
struct Volume {
	#[serde(rename = "box")]
    box_v: Vec<f64>
}

#[derive(Serialize,Clone,Default)]
struct Content {
	boundingVolume: Volume,
    url: String
}

#[derive(Default)]
struct Tile {
    boundingVolume: Volume,
    content: Content,
    geometricError: f64,
    children: Vec<Rc<RefCell<Tile>>>
}

#[derive(Serialize)]
struct TileJson {
    boundingVolume: Volume,
    content: Content,
    geometricError: f64,
    children: Vec<TileJson>
}

#[derive(Serialize)]
struct RootJson {
	refine: String,
	transform: Vec<f64>,
    boundingVolume: Volume,
    geometricError: f64,
    children: Vec<TileJson>
}

fn copy_tileset(tile: &Tile) -> TileJson{
	let t = TileJson {
		boundingVolume: tile.boundingVolume.clone(),
		content: tile.content.clone(),
		geometricError: tile.geometricError,
		children: {
			let mut ch = vec![];
			for x in tile.children.iter() {
				let t1 = copy_tileset(&x.borrow_mut());
				ch.push(t1);
			}
			ch
		}
	};
	t
}

fn find_coor_num(str: &str, lvl: usize) -> String {
	let mut lvl_str = String::from("_L");
	lvl_str += &lvl.to_string();
	lvl_str += "_";
	let xx_pos = str.rfind(&lvl_str).unwrap();
	let bin_path_buf = str.as_bytes();
	let mut start_pos = xx_pos + lvl_str.len();
	let mut coor_str = vec![];
	loop {
		if bin_path_buf[start_pos] < b'0' 
		|| bin_path_buf[start_pos] > b'9'{
			break;
		}
		coor_str.push(bin_path_buf[start_pos]);
		start_pos += 1;
	}
	String::from_utf8(coor_str).unwrap()
}

fn find_lvl_num(str: &str) -> String {
	let mut valid_num = String::new();
	if let Some(pos) = str.rfind("_L") {
		let slice = str.as_bytes();
		let lvl_str = String::from_utf8(slice.get(pos+2..pos+4).unwrap().into()).unwrap();
		for x in lvl_str.chars() {
			if x.is_digit(10) {
				valid_num.push(x);
			}
		}
	}
	valid_num
}

fn string_r_replace(dest :&str, str0: &str, str1: &str) -> Option<String> {
	if let Some(pos) = dest.rfind(str0) {
		let v_str = dest.as_bytes();
		let mut new_str = v_str.get(0..pos).unwrap().to_vec();
		let mut v_str1 = str1.as_bytes().to_vec();
		new_str.append(&mut v_str1);
		let mut subfix = v_str.get((pos+str0.len())..).unwrap().to_vec();
		new_str.append(&mut subfix);
		Some(String::from_utf8(new_str).unwrap())
	}
	else {
		None
	}
}

fn get_geometric_error(center_y: f64, lvl: i32) -> f64 {
	use std::f64;
	use std::i32;
	let x = center_y * f64::consts::PI / 180.0;
	let round = x.cos() * 2.0 * f64::consts::PI * 6378137.0;
	let pow = 2i32.pow(lvl as u32 - 2);
	4.0 * round / ( 256 * pow) as f64 
}

pub fn merge_osgb_tileset(
	path_root: &Path, center_x: f64, 
	center_y: f64, region_offset: Option<f64>
	) {
	use std::ffi::OsStr;
	use std::collections::BTreeMap;
	use std::io::prelude::*;
	use std::fs::File;
	use serde_json::{self,Value};
	
	let path = path_root.join("Data");
	// 指定 .\Data 目录
	if !path.exists() || !path.is_dir() { return }
	let mut root_tile = Tile::default();
	// minx,miny,minz,maxx,maxy,maxz
	let mut root_box = vec![1.0E+38f64,1.0E+38,1.0E+38,-1.0E+38,-1.0E+38,-1.0E+38];
	let mut path_find_tile = BTreeMap::<String, Rc<RefCell<Tile>>>::new();
	for x in path.read_dir().unwrap() {
		// every Tile_
		let entry = x.unwrap();
		let path = entry.path();
		let stem = path.file_stem().unwrap().to_str().unwrap();
		if !path.is_dir() || !stem.starts_with("Tile_") {
			continue;
		}
		// this is a tile
		let mut kv_path = BTreeMap::new();
		for osg in path.read_dir().unwrap() {
			let entry_osg = osg.unwrap();
			let osg_name = 
			{
				entry_osg.path()
				.file_name().unwrap()
				.to_str().unwrap().to_string()
			};
			if !osg_name.contains(".json") || entry_osg.path().is_dir() {
				continue;
			}
			let valid_num = find_lvl_num(&osg_name);
			if valid_num.len() > 0 {
				let lvl_num: usize = valid_num.parse().unwrap();
				kv_path.entry(lvl_num)
					.or_insert(vec![])
					.push(
						entry_osg.path()
							.to_str()
							.unwrap()
							.to_string()
					);
			}
		}
		if kv_path.is_empty() { continue; }
		let first_kv = kv_path.iter().next().unwrap();
		let first_lvl = first_kv.0;
		let first_lvl_coord_num = find_coor_num(first_kv.1[0].as_str(), *first_lvl).len();
		if first_lvl_coord_num != 1 {
			return;
		}
		for (lvl, v_path) in kv_path.iter() {
			for path in v_path {
				let mut f = File::open(path).unwrap();
				let mut buffer = String::new();
				f.read_to_string(&mut buffer).unwrap();
				let v: Value = serde_json::from_str(&buffer).unwrap();
				let box_v : Vec<f64> =	v["root"]["boundingVolume"]["box"]
							.as_array().unwrap().iter()
							.map(|x| x.as_f64().unwrap()).collect();
				let tile = Rc::new(RefCell::new(Tile{
					boundingVolume: Volume {
						box_v: box_v.clone()
					},
					content : Content {
						boundingVolume: Volume {
							box_v: box_v
						},
						url : {
							let v_str = v["root"]["content"]["url"].as_str().unwrap().to_string();
							format!("Data/{}/{}", stem , v_str)
						}
					},
					geometricError : get_geometric_error(center_y,*lvl as i32),//v["root"]["geometricError"].as_f64().unwrap(),
					children: vec![]
				}));
				// we must get the right key
				let xx_pos = path.rfind("_L").unwrap();
				let tile_coord_num : usize = (lvl - first_lvl + first_lvl_coord_num) as usize;
				let bin_path_buf = path.as_bytes();
				let mut start_pos = xx_pos + 2;
				loop {
					start_pos += 1;
					if bin_path_buf[start_pos] == b'_' {
						start_pos += 1;
						break;
					}
				}
				let end_pos = start_pos + tile_coord_num;
				let key_str = String::from_utf8(
						bin_path_buf.get(0..end_pos)
						.unwrap().into()
					).unwrap();
				//println!("{:?}", key_str);
				if first_lvl == lvl {
					root_tile.children.push(tile.clone());
					// modify the root 
					let t_b_r = &tile.borrow_mut().boundingVolume.box_v;
					root_box[0] = root_box[0].min(t_b_r[0] - t_b_r[3]);
					root_box[1] = root_box[1].min(t_b_r[1] - t_b_r[7]);
					root_box[2] = root_box[2].min(t_b_r[2] - t_b_r[11]);
					root_box[3] = root_box[3].max(t_b_r[0] + t_b_r[3]);
					root_box[4] = root_box[4].max(t_b_r[1] + t_b_r[7]);
					root_box[5] = root_box[5].max(t_b_r[2] + t_b_r[11]);
				}
				else {
					// find the parent
					let mut parent_path = key_str.clone().into_bytes();
					parent_path.pop();
					parent_path.pop();
					parent_path.push(b'0');
					let mut parent_path = String::from_utf8(parent_path).unwrap();
					let str0 = format!("_L{}_", lvl);
					let str1 = format!("_L{}_", lvl - 1);
					parent_path = string_r_replace(&parent_path, &str0, &str1).unwrap();
					if let Some(parent_tile) = path_find_tile.get_mut(&parent_path) {
						(*parent_tile).borrow_mut().children.push(tile.clone());
					}
				}
				//println!("{:?}", key_str);
				path_find_tile.insert(key_str, tile.clone());
			}
		}
	}
	// do merge plz
	let mut tras_height = 0f64;
	if let Some(v) = region_offset { 
		tras_height = v - root_box[2]; 
	}
	let mut trans_vec = vec![0f64; 16];
	unsafe {
		transform_c(center_x, center_y, tras_height, trans_vec.as_mut_ptr());	
	}
	// 
	let mut root_json = RootJson {
		refine: "REPLACE".into(),
		transform: trans_vec,
	    boundingVolume: Volume { box_v: box_to_tileset_box(&root_box) },
	    //content: Content { url: "".into() },
	    geometricError: get_geometric_error(center_y, 15),
	    children: vec![]
	};
	for x_tile in root_tile.children.iter() {
		let tile_json = copy_tileset(&x_tile.borrow_mut());
		root_json.children.push(tile_json);
	}
	let json_str = serde_json::to_string(&root_json).unwrap();
	let root_geometricError = get_geometric_error(center_y, 15);
	let mut tileset_json = String::new();
	tileset_json += r#"{"asset": {    "version": "0.0",    "gltfUpAxis": "Y"  },  "geometricError":"#;
	tileset_json += root_geometricError.to_string().as_str();
	tileset_json += r#","root": "#;
	tileset_json += json_str.as_str();
	tileset_json += "}";
	let path_json = path.with_file_name("tileset.json");
	let mut f = File::create(path_json).unwrap();
	f.write_all(tileset_json.as_bytes());
}

fn box_to_tileset_box(box_v: &Vec<f64>) -> Vec<f64> {
	let mut box_new = vec![];
	box_new.push( (box_v[0] + box_v[3]) / 2.0 );
	box_new.push( (box_v[1] + box_v[4]) / 2.0 );
	box_new.push( (box_v[2] + box_v[5]) / 2.0 );
	
	box_new.push( (box_v[3] - box_v[0]) / 2.0 );
	box_new.push( 0.0 );
	box_new.push( 0.0 );

	box_new.push( 0.0 );
	box_new.push( (box_v[4] - box_v[1]) / 2.0 );
	box_new.push( 0.0 );

	box_new.push( 0.0 );
	box_new.push( 0.0 );
	box_new.push( (box_v[5] - box_v[2]) / 2.0 );

	box_new
}