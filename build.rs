extern crate cmake;
extern crate pkg_config;
use cmake::Config;
use std::{env, fs, io, path::Path};

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

    let vcpkg_share_dir = "./vcpkg_installed/x64-windows/share";
    copy_gdal_data(vcpkg_share_dir);
    copy_proj_data(vcpkg_share_dir);
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

    let vcpkg_share_dir = "./vcpkg_installed/x64-linux-release/share";
    copy_gdal_data(vcpkg_share_dir);
    copy_proj_data(vcpkg_share_dir);
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

    let vcpkg_share_dir = "./vcpkg_installed/arm64-osx/share";
    copy_gdal_data(vcpkg_share_dir);
    copy_proj_data(vcpkg_share_dir);
}

fn link_compile_commands() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let build_dir = format!("{}/build", out_dir);
    let cc_path = format!("{}/compile_commands.json", build_dir);

    // 拷到项目根目录（或固定目录）
    let project_root = env::var("CARGO_MANIFEST_DIR").unwrap();
    let target_dir = format!("{}/.vscode", project_root);
    match fs::create_dir(target_dir.as_str()) {
        Ok(_) => {}
        Err(e) => {
            if e.kind() != io::ErrorKind::AlreadyExists {
                panic!("{}", e);
            }
        }
    };
    let target_path = format!("{}/compile_commands.json", target_dir);

    let _ = fs::copy(&cc_path, &target_path);
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

    link_compile_commands();
}
