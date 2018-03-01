extern crate cc;
use std::fs;
use std::process::{Command,Stdio};

fn main() {
    fs::create_dir_all("target/debug").unwrap();
    fs::create_dir_all("target/release").unwrap();
    
    if cfg!(windows) {
        cc::Build::new()
            .cpp(true)
            .warnings(false)
            .define("WIN32",None)
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

        let out = Command::new("cmd")
            .args(
                &[
                    "/C",
                    "xcopy",
                    r#".\bin"#,
                    r#".\target\debug"#,
                    "/y",
                    "/e"
                ],
            )
            .stdout(Stdio::inherit())
            .output()
            .expect("fuck");
        Command::new("cmd")
            .args(
                &[
                    "/C",
                    "xcopy",
                    r#".\bin"#,
                    r#".\target\release"#,
                    "/y",
                    "/e"
                ],
            )
            .stdout(Stdio::inherit())
            .output()
            .expect("fuck");
    } else {
        cc::Build::new()
            .cpp(true)
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
        println!("cargo:rustc-link-lib=osg");
        println!("cargo:rustc-link-lib=osgDB");
        println!("cargo:rustc-link-lib=osgUtil");

        let out = Command::new("cp")
            .args(
                &[
                    r#".\bin\linux"#,
                    r#".\target\debug"#,
                    "-r"
                ],
            )
            .stdout(Stdio::inherit())
            .output();
        Command::new("cp")
            .args(
                &[
                    r#".\bin\linux"#,
                    r#".\target\release"#,
                    "-r"
                ],
            )
            .stdout(Stdio::inherit())
            .output();
    }
}
