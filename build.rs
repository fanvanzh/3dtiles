extern crate cmake;
extern crate pkg_config;
use cmake::Config;
use std::env;

fn build_win_msvc() {
    // Probe Library Link for GDAL and OpenSceneGraph
    pkg_config::Config::new().atleast_version("3.9.3").probe("gdal").unwrap();
    pkg_config::Config::new().atleast_version("3.6.5").probe("OpenSceneGraph").unwrap();
    // Get VCPKG_ROOT environment variable
    let vcpkg_root = std::env::var("VCPKG_ROOT").unwrap();
    let dst = Config::new(".").define("CMAKE_TOOLCHAIN_FILE", format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root)).very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // Link Search Path for Third Party Libraries
    let vcpkg_installed_dir = "./vcpkg_installed/x64-windows/lib";
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_dir);
    // openscenegraph library
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");
}

fn build_linux_unkown() {
    // Probe Library Link for GDAL and OpenSceneGraph
    pkg_config::Config::new().atleast_version("3.9.2").probe("gdal").unwrap();
    pkg_config::Config::new().atleast_version("3.6.5").probe("OpenSceneGraph").unwrap();
    // Get VCPKG_ROOT environment variable
    let vcpkg_root = std::env::var("VCPKG_ROOT").unwrap();
    let dst = Config::new(".").define("CMAKE_TOOLCHAIN_FILE", format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root)).very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // for C++ Standard Library
    println!("cargo:rustc-link-lib=stdc++");
    // Link Search Path for Third Party Libraries
    let vcpkg_installed_dir = "./vcpkg_installed/x64-linux-release/lib";
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_dir);
    // openscenegraph library
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");
}

fn build_macos() {
    // Probe Library Link for GDAL and OpenSceneGraph
    pkg_config::Config::new().atleast_version("3.9.2").probe("gdal").unwrap();
    pkg_config::Config::new().atleast_version("3.6.5").probe("OpenSceneGraph").unwrap();
    // Get VCPKG_ROOT environment variable
    let vcpkg_root = std::env::var("VCPKG_ROOT").unwrap();
    let dst = Config::new(".").define("CMAKE_TOOLCHAIN_FILE", format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root)).very_verbose(true).build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");
    // for C++ Standard Library
    println!("cargo:rustc-link-lib=c++");
    let vcpkg_installed_dir = "./vcpkg_installed/arm64-osx/lib/";
    // Link Search Path for Third Party Libraries
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_dir);
    // openscenegraph library
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
            "x86_64-unknown-linux-gnu" => build_linux_unkown(),
            "x86_64-pc-windows-msvc" => build_win_msvc(),
            "aarch64-apple-darwin" => build_macos(),
            &_ => {}
        },
        _ => {}
    }
}
