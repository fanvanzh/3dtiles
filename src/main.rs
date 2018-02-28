extern crate libc;
extern crate encoding;
extern crate rayon;
extern crate serde;

extern crate serde_json;
#[macro_use]
extern crate serde_derive;

mod osgb;
mod shape;
pub mod fun_c;

fn main() {
	// use std::io;
	// let mut msg = String::new();
	// io::stdin().read_line(&mut msg).unwrap();
	// let shpfile = r#"E:\test\buildings\osm_bd_height_rd.shp"#;
	// let dest = r#"E:\test\buildings\结果文件"#;
	// let res_tile = visit_path(Path::new(dest));
	// println!("{:#?}", res_tile.unwrap());
	// let mut rs = GB18030.encode(shpfile, EncoderTrap::Strict).unwrap();
	// shape_batch_convert(shpfile, dest);

	use std::time;

	let dir = std::path::Path::new(r#"E:\Data\倾斜摄影\hgc\"#);
	let dir_dest = std::path::Path::new(r#"E:\Data\倾斜摄影\hgc_b3dm\"#);
	let dir_tileset = dir_dest.join("Data");
	let center_x = 120.0;
	let center_y = 30.0;
	let trans_region = Some(0.0);
	let tick = time::SystemTime::now();
	osgb::osgb_batch_convert(&dir,&dir_dest, center_x, center_y);
	osgb::merge_osgb_tileset(
		&dir_tileset, 
		center_x, center_y, trans_region
	);
	println!("{:?}", tick.elapsed());
}

