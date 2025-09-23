extern crate cmake;
use std::{env, fs, io, path::Path};

use cmake::Config;

fn build_win_msvc() {
    // Get VCPKG_ROOT environment variable
    let vcpkg_root = std::env::var("VCPKG_ROOT").unwrap();
    let dst = Config::new(".").define("CMAKE_TOOLCHAIN_FILE", format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root)).very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // Link Search Path for Third Party Libraries
    let vcpkg_installed_dir = "./vcpkg/installed/x64-windows-release/lib";
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_dir);
    // openscenegraph library
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");

    let vcpkg_share_dir = "./vcpkg/installed/x64-windows-release/share";
    copy_gdal_data(vcpkg_share_dir);
    copy_proj_data(vcpkg_share_dir);
}

fn build_linux_unkown() {
    // Probe Library Link for GDAL and OpenSceneGraph
    pkg_config::Config::new().atleast_version("3.8.4").probe("gdal").unwrap();
    pkg_config::Config::new().probe("openscenegraph").unwrap();
    let dst = Config::new(".").very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // -------------
    println!("cargo:rustc-link-search=native=/usr/lib/x86_64-linux-gnu");
    println!("cargo:rustc-link-search=native=./vcpkg/installed/x64-linux-release/lib");
    // -------------
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");
    // Link Standard C++
    println!("cargo:rustc-link-lib=stdc++");

    // for ubuntu 22.04 using apt install libgdal-dev
    copy_gdal_data("/usr/share/");
    copy_proj_data("/usr/share/");
}

fn build_macos() {
    // Probe Library Link for GDAL and OpenSceneGraph
    pkg_config::Config::new().atleast_version("3.8.4").probe("gdal").unwrap();
    pkg_config::Config::new().atleast_version("3.6.5").probe("OpenSceneGraph").unwrap();
    // Get VCPKG_ROOT environment variable
    let dst = Config::new(".").very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // Link search paths
    println!("cargo:rustc-link-search=native=/opt/homebrew/lib");
    println!("cargo:rustc-link-search=native=/opt/homebrew/opt/gdal/lib");
    println!("cargo:rustc-link-search=native=/opt/homebrew/opt/open-scene-graph/lib");

    // OSG libraries
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");

    // GDAL library
    println!("cargo:rustc-link-lib=gdal");
    // Link Standard C++
    println!("cargo:rustc-link-lib=c++");
    copy_gdal_data("/opt/homebrew/opt/gdal/share/");
    copy_proj_data("/opt/homebrew/opt/proj/share/");
}

fn copy_gdal_data(share: &str) {
    let profile = env::var("PROFILE").unwrap();
    let gdal_data = Path::new(share).join("gdal");
    let out_dir = Path::new(&env::var("CARGO_TARGET_DIR")
    .unwrap_or("target".into()))
    .join(&profile)
    .join("gdal");

    copy_dir_recursive(&gdal_data, &out_dir).unwrap();
}

fn copy_dir_recursive(src: &Path, dst: &Path) -> io::Result<()> {
    if !dst.exists() {
        fs::create_dir_all(dst)?;
    }

    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let path = entry.path();
        let dest_path = dst.join(entry.file_name());

        if path.is_dir() {
            copy_dir_recursive(&path, &dest_path)?;
        } else {
            fs::copy(&path, &dest_path)?;
        }
    }
    Ok(())
}

fn copy_proj_data(share: &str) {
    let profile = env::var("PROFILE").unwrap();
    let proj_data = Path::new(share).join("proj");
    let out_dir = Path::new(&env::var("CARGO_TARGET_DIR")
    .unwrap_or("target".into()))
    .join(&profile)
    .join("proj");

    copy_dir_recursive(&proj_data, &out_dir).unwrap();
}

fn main() {
    match env::var("TARGET") {
        Ok(val) => match val.as_str() {
            "x86_64-unknown-linux-gnu" => build_linux_unkown(),
            "x86_64-pc-windows-msvc" => build_win_msvc(),
            "aarch64-apple-darwin" => build_macos(),
            &_ => {}
        },
        _ => {}
    }
}
