extern crate cc;
use std::env;

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
        .include("./vcpkg/installed/x64-windows/include")
        .file("./src/tileset.cpp")
        .file("./src/shp23dtile.cpp")
        .file("./src/osgb23dtile.cpp")
        .file("./src/dxt_img.cpp")
        .file("./src/GeoTransform.cpp")
        .compile("_3dtile");
    // -------------
    println!("cargo:rustc-link-search=native=./vcpkg/installed/x64-windows/lib");
    // ------ GDAL library -------
    println!("cargo:rustc-link-lib=gdal");
    // ------ OSG library --------
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgViewer");
    println!("cargo:rustc-link-lib=OpenThreads");
}

fn build_linux_unkonw() {
    cc::Build::new()
        .cpp(true)
        .flag("-std=c++11")
        .warnings(false)
        .include("./src")
        .include("./vcpkg/installed/x64-linux/include")
        .file("./src/tileset.cpp")
        .file("./src/shp23dtile.cpp")
        .file("./src/osgb23dtile.cpp")
        .file("./src/dxt_img.cpp")
        .file("./src/GeoTransform.cpp")
        .compile("_3dtile");
    // -------------
    println!("cargo:rustc-link-search=native=/usr/lib/x86_64-linux-gnu");
    println!("cargo:rustc-link-search=native=./vcpkg/installed/x64-linux/lib");
    // -------------
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
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
