extern crate clap;
extern crate serde;
#[macro_use]
extern crate serde_json;
extern crate serde_xml_rs;
#[macro_use]
extern crate log;
extern crate byteorder;
extern crate chrono;
extern crate env_logger;
extern crate libc;

pub mod fun_c;
mod osgb;
mod shape;

use chrono::prelude::*;
use clap::{Arg, ArgAction, Command};
use log::LevelFilter;
use serde::Deserialize;
use std::io::Write;

/// Setup OpenSceneGraph environment variables for plugin loading
fn setup_osg_environment() {
    use std::env;

    // Get the executable directory
    let exe_path = env::current_exe().ok();
    let exe_dir = exe_path.as_ref().and_then(|p| p.parent());

    // Try to set OSG_LIBRARY_PATH if not already set
    if env::var("OSG_LIBRARY_PATH").is_err() {
        if let Some(dir) = exe_dir {
            // Check for osgPlugins directory next to the executable
            let plugins_dir = dir.join("osgPlugins-3.6.5");
            if plugins_dir.exists() {
                unsafe { env::set_var("OSG_LIBRARY_PATH", &plugins_dir) };
                info!("OSG_LIBRARY_PATH set to: {:?}", plugins_dir);
            } else {
                // Fallback: try to find plugins in common locations
                let alt_plugins = dir.join("lib").join("osgPlugins-3.6.5");
                if alt_plugins.exists() {
                    unsafe { env::set_var("OSG_LIBRARY_PATH", &alt_plugins) };
                    info!("OSG_LIBRARY_PATH set to: {:?}", alt_plugins);
                }
            }
        }
    }

    // Setup GDAL_DATA and PROJ_DATA paths
    if env::var("GDAL_DATA").is_err() {
        if let Some(dir) = exe_dir {
            let gdal_data = dir.join("gdal");
            if gdal_data.exists() {
                unsafe { env::set_var("GDAL_DATA", &gdal_data) };
            }
        }
    }

    if env::var("PROJ_DATA").is_err() {
        if let Some(dir) = exe_dir {
            let proj_data = dir.join("proj");
            if proj_data.exists() {
                unsafe { env::set_var("PROJ_DATA", &proj_data) };
            }
        }
    }
}

fn main() {
    use std::env;

    // Setup OSG plugin path for runtime plugin loading
    setup_osg_environment();

    if let Err(_) = env::var("RUST_LOG") {
        unsafe { env::set_var("RUST_LOG", "info") };
    }
    unsafe { env::set_var("RUST_BACKTRACE", "1") };
    let mut builder = env_logger::Builder::from_default_env();
    builder
        .format(|buf, record| {
            let dt = Local::now();
            writeln!(
                buf,
                "{}: {} - {}",
                record.level(),
                dt.format("%Y-%m-%d %H:%M:%S").to_string(),
                record.args()
            )
        })
        .filter(None, LevelFilter::Info)
        .init();
    //env_logger::init();
    let matches = Command::new("Make 3dtile program")
        .version("1.0")
        .author("fanvanzh <fanvanzh@sina.com>")
        .about("a very fast 3dtile tool")
        .arg(
            Arg::new("input")
                .short('i')
                .long("input")
                .value_name("FILE")
                .help("Set the input file")
                .required(true)
                .num_args(1),
        )
        .arg(
            Arg::new("output")
                .short('o')
                .long("output")
                .value_name("FILE")
                .help("Set the out file")
                .required(true)
                .num_args(1),
        )
        .arg(
            Arg::new("format")
                .short('f')
                .long("format")
                .value_name("osgb,shape,gltf,b3dm")
                .help("Set input format")
                .required(true)
                .value_parser(["osgb", "shape", "gltf", "b3dm"])
                .num_args(1),
        )
        .arg(
            Arg::new("config")
                .short('c')
                .long("config")
                .help(
                    "Set the tile config:
{
    \"x\": x,
    \"y\": y,
    \"offset\": 0,
    \"max_lvl\" : 20,
    \"pbr\" : false,
}",
                )
                .num_args(1),
        )
        .arg(
            Arg::new("height")
                .long("height")
                .help("Set the shapefile height field")
                .num_args(1),
        )
        .arg(
            Arg::new("verbose")
                .short('v')
                .long("verbose")
                .help("Set output verbose ")
                .action(ArgAction::SetTrue),
        )
        .arg(
            Arg::new("enable-draco")
                .long("enable-draco")
                .help("Enable Draco mesh compression")
                .action(ArgAction::SetTrue),
        )
        .arg(
            Arg::new("enable-simplify")
                .long("enable-simplify")
                .help("Enable mesh simplification")
                .action(ArgAction::SetTrue),
        )
        .arg(
            Arg::new("enable-texture-compress")
                .long("enable-texture-compress")
                .help("Enable texture compression (KTX2)")
                .action(ArgAction::SetTrue),
        )
        .arg(
            Arg::new("enable-lod")
                .long("enable-lod")
                .help("Enable LOD (Level of Detail) with default configuration")
                .action(ArgAction::SetTrue),
        )
        .get_matches();

    let input = matches
        .get_one::<String>("input")
        .expect("input is required")
        .as_str();
    let output = matches
        .get_one::<String>("output")
        .expect("output is required")
        .as_str();
    let format = matches
        .get_one::<String>("format")
        .expect("format is required")
        .as_str();
    let tile_config = matches
        .get_one::<String>("config")
        .map(|s| s.as_str())
        .unwrap_or("");
    let height_field = matches
        .get_one::<String>("height")
        .map(|s| s.as_str())
        .unwrap_or("");

    // Parse feature flags
    let enable_draco = matches.get_flag("enable-draco");
    let enable_simplify = matches.get_flag("enable-simplify");
    let enable_texture_compress = matches.get_flag("enable-texture-compress");
    let enable_lod = matches.get_flag("enable-lod");

    if matches.get_flag("verbose") {
        info!("set program versose on");
    }
    if enable_draco {
        info!("Draco compression enabled");
    }
    if enable_simplify {
        info!("Mesh simplification enabled");
    }
    if enable_texture_compress {
        info!("Texture compression (KTX2) enabled");
    }
    if enable_lod {
        info!("LOD (Level of Detail) enabled with default configuration [1.0, 0.5, 0.25]");
    }

    let in_path = std::path::Path::new(input);
    if !in_path.exists() {
        error!("{} does not exists.", input);
        return;
    }
    match format {
        "osgb" => {
            convert_osgb(input, output, tile_config, enable_simplify, enable_texture_compress, enable_draco);
        }
        "shape" => {
            convert_shapefile(
                input,
                output,
                height_field,
                enable_lod,
                enable_simplify,
                enable_draco,
            );
        }
        "gltf" => {
            convert_gltf(input, output);
        }
        "b3dm" => {
            convert_b3dm(input, output);
        }
        _ => {
            error!("not support now.");
        }
    }
}

fn convert_b3dm(src: &str, dest: &str) {
    use std::fs::File;
    use std::io::prelude::*;
    use std::path::Path;

    use byteorder::{LittleEndian, ReadBytesExt};
    use std::io::Cursor;
    //use std::io::SeekFrom;

    if !dest.ends_with(".gltf") && !dest.ends_with(".glb") {
        error!("output format not support now: {}", dest);
        return;
    }
    if !src.ends_with(".b3dm") {
        error!("input format must be b3dm");
    }
    if Path::new(src).exists() && Path::new(src).is_file() {
        if let Ok(mut f) = File::open(src) {
            let mut buffer = Vec::new();
            f.read_to_end(&mut buffer).unwrap();
            let mut rdr = Cursor::new(buffer);
            let offset = {
                rdr.set_position(12);
                let fj_len = rdr.read_u32::<LittleEndian>().unwrap();
                rdr.read_u32::<LittleEndian>().unwrap();
                let bj_len = rdr.read_u32::<LittleEndian>().unwrap();
                let offset = fj_len + bj_len + 28;
                offset as usize
            };
            if let Ok(mut df) = File::create(dest) {
                let buf = rdr.get_ref();
                df.write_all(&buf.as_slice()[offset..]).unwrap();
            }
        }
    }
    info!("task over");
}

// convert any thing to gltf
fn convert_gltf(src: &str, dest: &str) {
    use std::ffi::CString;
    if !dest.ends_with(".gltf") && !dest.ends_with(".glb") {
        error!("output format not support now: {}", dest);
        return;
    }
    if !src.ends_with(".osgb")
        && !src.ends_with(".osg")
        && !src.ends_with(".obj")
        && !src.ends_with(".fbx")
        && !src.ends_with(".3ds")
    {
        error!("input format not support now: {}", src);
        return;
    }
    unsafe {
        let c_str = CString::new(dest).unwrap();
        let ret = osgb::osgb2glb(src.as_ptr(), c_str.as_ptr() as *const u8);
        if !ret {
            error!("convert failed");
        } else {
            info!("task over");
        }
    }
}

#[allow(dead_code)]
#[allow(non_snake_case)]
#[derive(Debug, Deserialize)]
struct ModelMetadata {
    pub version: String,
    pub SRS: String,
    pub SRSOrigin: String,
}

fn convert_osgb(src: &str, dest: &str, config: &str, enable_simplify: bool, enable_texture_compress: bool, enable_draco: bool) {
    use serde_json::Value;
    use std::fs::File;
    use std::io::prelude::*;
    use std::time;

    let dir = std::path::Path::new(src);
    let dir_dest = std::path::Path::new(dest);

    let mut center_x = 0f64;
    let mut center_y = 0f64;
    let mut max_lvl = None;
    let mut trans_region = None;
    let enu_offset: Option<(f64, f64, f64)> = None;
    let mut origin_height: Option<f64> = None;

    // try parse metadata.xml
    let metadata_file = dir.join("metadata.xml");
    if metadata_file.exists() {
        // read and parse
        if let Ok(mut f) = File::open(&metadata_file) {
            let mut buffer = String::new();
            if let Ok(_) = f.read_to_string(&mut buffer) {
                //
                if let Ok(metadata) = serde_xml_rs::from_str::<ModelMetadata>(&buffer) {
                    //println!("{:?}", metadata);
                    let v: Vec<&str> = metadata.SRS.split(":").collect();
                    if v.len() > 1 {
                        if v[0] == "ENU" {
                            let v1: Vec<&str> = v[1].split(",").collect();
                            if v1.len() > 1 {
                                let v1_num = (*v1[0]).parse::<f64>();
                                let v2_num = v1[1].parse::<f64>();
                                if v1_num.is_ok() && v2_num.is_ok() {
                                    center_y = v1_num.unwrap();
                                    center_x = v2_num.unwrap();

                                    // Parse and apply SRSOrigin offset
                                    let origin_parts: Vec<&str> =
                                        metadata.SRSOrigin.split(",").collect();
                                    if origin_parts.len() >= 2 {
                                        if let (Ok(offset_x), Ok(offset_y)) = (
                                            origin_parts[0].parse::<f64>(),
                                            origin_parts[1].parse::<f64>(),
                                        ) {
                                            // Parse Z offset (height) if available
                                            let offset_z = if origin_parts.len() >= 3 {
                                                origin_parts[2].parse::<f64>().unwrap_or(0.0)
                                            } else {
                                                0.0
                                            };

                                            // Call enu_init to set up GeoTransform for geometry correction
                                            let gdal_data: String = {
                                                use std::path::Path;
                                                let exe_dir = ::std::env::current_exe().unwrap();
                                                Path::new(&exe_dir)
                                                    .parent()
                                                    .unwrap()
                                                    .join("gdal")
                                                    .to_str()
                                                    .unwrap()
                                                    .into()
                                            };
                                            let proj_lib: String = {
                                                use std::path::Path;
                                                let exe_dir = ::std::env::current_exe().unwrap();
                                                Path::new(&exe_dir)
                                                    .parent()
                                                    .unwrap()
                                                    .join("proj")
                                                    .to_str()
                                                    .unwrap()
                                                    .into()
                                            };

                                            unsafe {
                                                use std::ffi::CString;
                                                let mut origin_enu = vec![offset_x, offset_y, offset_z];
                                                let gdal_c_str = CString::new(gdal_data).unwrap();
                                                let gdal_ptr = gdal_c_str.as_ptr();
                                                let proj_c_str = CString::new(proj_lib).unwrap();
                                                let proj_ptr = proj_c_str.as_ptr();
                                                if !osgb::enu_init(center_x, center_y, origin_enu.as_mut_ptr(), gdal_ptr, proj_ptr) {
                                                    error!("enu_init failed!");
                                                }
                                            }

                                            // For ENU systems, use height=0 (or terrain elevation) for root transform
                                            // The SRSOrigin offset is already baked into the tile geometry coordinates
                                            origin_height = Some(0.0);

                                            info!("ENU SRSOrigin offset detected: x={}, y={}, z={}", offset_x, offset_y, offset_z);
                                            info!("Using geographic origin for transform: lon={}, lat={}, h=0", center_x, center_y);
                                        } else {
                                            error!("Failed to parse SRSOrigin values");
                                        }
                                    } else {
                                        error!("SRSOrigin format invalid, expected x,y,z");
                                    }
                                } else {
                                    error!("parse ENU point error");
                                }
                            } else {
                                error!("ENU point is not enough");
                            }
                        } else if v[0] == "EPSG" {
                            // call gdal to convert
                            if let Ok(srs) = v[1].parse::<i32>() {
                                let mut pt: Vec<f64> = metadata
                                    .SRSOrigin
                                    .split(",")
                                    .map(|v| v.parse().unwrap())
                                    .collect();
                                if pt.len() >= 2 {
                                    let gdal_data: String = {
                                        use std::path::Path;
                                        let exe_dir = ::std::env::current_exe().unwrap();
                                        Path::new(&exe_dir)
                                            .parent()
                                            .unwrap()
                                            .join("gdal")
                                            .to_str()
                                            .unwrap()
                                            .into()
                                    };
                                    let proj_lib: String = {
                                        use std::path::Path;
                                        let exe_dir = ::std::env::current_exe().unwrap();
                                        Path::new(&exe_dir)
                                            .parent()
                                            .unwrap()
                                            .join("proj")
                                            .to_str()
                                            .unwrap()
                                            .into()
                                    };
                                    unsafe {
                                        use std::ffi::CString;
                                        let gdal_c_str = CString::new(gdal_data).unwrap();
                                        let gdal_ptr = gdal_c_str.as_ptr();
                                        let proj_c_str = CString::new(proj_lib).unwrap();
                                        let proj_ptr = proj_c_str.as_ptr();
                                        if osgb::epsg_convert(srs, pt.as_mut_ptr(), gdal_ptr, proj_ptr) {
                                            center_x = pt[0];
                                            center_y = pt[1];
                                            // Store height from original SRSOrigin (pt[2] if available)
                                            if pt.len() >= 3 {
                                                origin_height = Some(pt[2]);
                                                info!("epsg: x->{}, y->{}, h={}", pt[0], pt[1], pt[2]);
                                            } else {
                                                info!("epsg: x->{}, y->{}", pt[0], pt[1]);
                                            }
                                        } else {
                                            error!("epsg convert failed!");
                                        }
                                    }
                                } else {
                                    error!("epsg point is not enough");
                                }
                            } else {
                                error!("parse EPSG failed");
                            }
                        //
                        } else {
                            error!("EPSG or ENU is expected in SRS");
                        }
                    } else {
                        // error!("SRS content error");
                        // treat as wkt
                        let mut pt: Vec<f64> = metadata
                            .SRSOrigin
                            .split(",")
                            .map(|v| v.parse().unwrap())
                            .collect();
                        if pt.len() >= 2 {
                            let gdal_data: String = {
                                use std::path::Path;
                                let exe_dir = ::std::env::current_exe().unwrap();
                                Path::new(&exe_dir)
                                    .parent()
                                    .unwrap()
                                    .join("gdal_data")
                                    .to_str()
                                    .unwrap()
                                    .into()
                            };
                            unsafe {
                                use std::ffi::CString;
                                let wkt: String = metadata.SRS;
                                // println!("{:?}", wkt);
                                let c_str = CString::new(gdal_data).unwrap();
                                let ptr = c_str.as_ptr();
                                let wkt_cstr = CString::new(wkt).unwrap();
                                let wkt_ptr = wkt_cstr.as_ptr();
                                if osgb::wkt_convert(wkt_ptr, pt.as_mut_ptr(), ptr) {
                                    center_x = pt[0];
                                    center_y = pt[1];
                                    info!("wkt: x->{}, y->{}", pt[0], pt[1]);
                                } else {
                                    error!("wkt convert failed!");
                                }
                            }
                        }
                    }
                } else {
                    error!("parse {} failed", metadata_file.display());
                }
            } else {
                error!("read {} failed", metadata_file.display());
            }
        } else {
            error!("open {} failed", metadata_file.display());
        }
    } else {
        error!("{} is missing", metadata_file.display());
    }
    if let Ok(v) = serde_json::from_str::<Value>(config) {
        if let Some(x) = v["x"].as_f64() {
            center_x = x;
        }
        if let Some(y) = v["y"].as_f64() {
            center_y = y;
        }
        if let Some(h) = v["offset"].as_f64() {
            trans_region = Some(h);
        }
        if let Some(lvl) = v["max_lvl"].as_i64() {
            max_lvl = Some(lvl as i32);
        }
    } else if config.len() > 0 {
        error!("config error --> {}", config);
    }
    let tick = time::SystemTime::now();
    if let Err(e) = osgb::osgb_batch_convert(
        &dir, &dir_dest, max_lvl,
        center_x, center_y, trans_region,
        enu_offset, origin_height, enable_texture_compress, enable_simplify, enable_draco)
    {
        error!("{}", e);
        return;
    }
    let elap_sec = tick.elapsed().unwrap();
    let tick_num = elap_sec.as_secs() as f64 + elap_sec.subsec_nanos() as f64 * 1e-9;
    info!("task over, cost {:.2} s.", tick_num);
}

fn convert_shapefile(
    src: &str,
    dest: &str,
    height: &str,
    enable_lod: bool,
    enable_simplify: bool,
    enable_draco: bool,
) {
    if height.is_empty() {
        error!("you must set the height field by --height xxx");
        return;
    }
    let tick = std::time::SystemTime::now();

    let ret = shape::shape_batch_convert(
        src,
        dest,
        height,
        enable_lod,
        enable_simplify,
        enable_draco,
    );
    if !ret {
        error!("convert shapefile failed");
    } else {
        let elap_sec = tick.elapsed().unwrap();
        let tick_num = elap_sec.as_secs() as f64 + elap_sec.subsec_nanos() as f64 * 1e-9;
        info!("task over, cost {:.2} s.", tick_num);
    }
}
