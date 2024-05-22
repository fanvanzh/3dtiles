extern crate cc;
use std::env;
use std::process::{Command, Stdio};

fn build_win_msvc() {
    cc::Build::new()
        .cpp(true)
        .flag("-Zi")
        .flag("-Gm")
        .flag("-INCREMENTAL")
        .flag("-bigobj")
        .warnings(false)
        .define("WIN32", None)
        .define("_WINDOWS", None)
        .include("./src")
        .file("./src/tileset.cpp")
        .file("./src/shp23dtile.cpp")
        .file("./src/osgb23dtile.cpp")
        .file("./src/dxt_img.cpp")
        .file("./src/GeoTransform.cpp")
        .compile("3dtile");
    // -------------
    println!("cargo:rustc-link-search=native=./lib");
    // -------------
    println!("cargo:rustc-link-lib=gdal_i");
    println!("cargo:rustc-link-lib=OpenThreads");
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgViewer");

    Command::new("cmd")
        .args(&[
            "/C",
            "xcopy",
            r#".\bin"#,
            &format!(r#".\target\{}"#, env::var("PROFILE").unwrap()),
            "/y",
            "/e",
        ])
        .stdout(Stdio::inherit())
        .output()
        .unwrap();
}

fn build_linux_unkonw() {
    cc::Build::new()
        .cpp(true)
        .flag("-std=c++11")
        .warnings(false)
        .include("./src")
        .file("./src/tileset.cpp")
        .file("./src/shp23dtile.cpp")
        .file("./src/osgb23dtile.cpp")
        .file("./src/dxt_img.cpp")
        .file("./src/GeoTransform.cpp")
        .compile("3dtile");
    // -------------
    println!("cargo:rustc-link-search=native=/usr/lib/x86_64-linux-gnu");
    // -------------
    println!("cargo:rustc-link-lib=OpenThreads");
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");
}

fn main() {
    match env::var("TARGET") {
        Ok(val) => match val.as_str() {
            "x86_64-unknown-linux-gnu" => build_linux_unkonw(),
            "x86_64-pc-windows-msvc" => build_win_msvc(),
            &_ => {}
        },
        _ => {}
    }
}
