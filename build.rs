extern crate cc;
use std::fs;
use std::process::{Command, Stdio};

fn main() {
    fs::create_dir_all("target/debug").unwrap();
    fs::create_dir_all("target/release").unwrap();

    if cfg!(windows) {
        cc::Build::new()
            .cpp(true)
            .flag("-Zi")
            .flag("-Gm")
            .flag("-INCREMENTAL")
            .warnings(false)
            .define("WIN32", None)
            .include("./src")
            .include("./src/osg")
            .file("./src/tileset.cpp")
            .file("./src/shp23dtile.cpp")
            .file("./src/osgb23dtile.cpp")
            .compile("3dtile");
        // -------------
        println!("cargo:rustc-link-search=native=./lib");
        // -------------
        println!("cargo:rustc-link-lib=gdal_i");
        println!("cargo:rustc-link-lib=osg");
        println!("cargo:rustc-link-lib=osgDB");
        println!("cargo:rustc-link-lib=osgUtil");

        Command::new("cmd")
            .args(
                &["/C", "xcopy", r#".\bin"#, r#".\target\debug"#, "/y", "/e"],
            )
            .stdout(Stdio::inherit())
            .output()
            .unwrap();
        Command::new("cmd")
            .args(
                &["/C", "xcopy", r#".\bin"#, r#".\target\release"#, "/y", "/e"],
            )
            .stdout(Stdio::inherit())
            .output()
            .unwrap();
    } else {
        cc::Build::new()
            .cpp(true)
            .flag("-std=c++11")
            .warnings(false)
            .include("./src")
            .include("./src/osg")
            .file("./src/tileset.cpp")
//          .file("./src/shp23dtile.cpp")
            .file("./src/osgb23dtile.cpp")
            .compile("3dtile");
        // -------------
        println!("cargo:rustc-link-search=native=./lib");
        // -------------
        //      println!("cargo:rustc-link-lib=gdal");
        println!("cargo:rustc-link-lib=OpenThreads");
        println!("cargo:rustc-link-lib=osg");
        println!("cargo:rustc-link-lib=osgDB");
        println!("cargo:rustc-link-lib=osgUtil");
    }
}
