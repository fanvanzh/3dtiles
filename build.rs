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
    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:warning=out_dir = {}", &out_dir);
    print_vcpkg_tree(Path::new(&out_dir)).unwrap();
    // vcpkg_installed path
    let vcpkg_installed_dir = Path::new(&out_dir).join("build").join("vcpkg_installed").join("x64-windows");

    // Link Search Path for third party library
    let vcpkg_installed_lib_dir = vcpkg_installed_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_lib_dir.display());
    // openscenegraph library
    println!("cargo:rustc-link-lib=osg");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=OpenThreads");
    // gdal library
    println!("cargo:rustc-link-lib=gdal");

    // copy gdal and proj data
    let vcpkg_share_dir = vcpkg_installed_dir.join("share");
    copy_gdal_data(vcpkg_share_dir.to_str().unwrap());
    copy_proj_data(vcpkg_share_dir.to_str().unwrap());
}

fn get_target_dir() -> std::path::PathBuf {
    let profile = env::var("PROFILE").unwrap();
    let target_dir = Path::new(&env::var("CARGO_TARGET_DIR")
    .unwrap_or("target".into()))
    .join(&profile);
    target_dir
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
    let gdal_data = Path::new(share).join("gdal");
    let out_dir = get_target_dir()
    .join("gdal");

    println!("gdal_data -> {}, out_dir -> {}", gdal_data.display(),out_dir.display());
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
    let proj_data = Path::new(share).join("proj");
    let out_dir = get_target_dir()
    .join("proj");
        println!("proj_data -> {}, out_dir -> {}", proj_data.display(),out_dir.display());

    copy_dir_recursive(&proj_data, &out_dir).unwrap();
}

/// 打印 ./vcpkg_installed 下的目录树，用 cargo:warning 输出，这样能在 cargo build 输出中看到
fn print_vcpkg_tree(root: &Path) -> io::Result<()> {
    if !root.exists() {
        println!("cargo:warning=path '{}' does not exist", root.display());
        return Ok(());
    }
    fn walk(path: &Path, prefix: &str) -> io::Result<()> {
        let meta = fs::metadata(path)?;
        if meta.is_dir() {
            println!("cargo:warning={}{}", prefix, path.file_name().map(|s| s.to_string_lossy()).unwrap_or_else(|| path.display().to_string().into()));
            let mut entries: Vec<_> = fs::read_dir(path)?.collect();
            entries.sort_by_key(|e| e.as_ref().map(|e| e.file_name()).ok());
            for entry in entries {
                let entry = entry?;
                let p = entry.path();
                if p.is_dir() {
                    walk(&p, &format!("{}  ", prefix))?;
                } else {
                    println!("cargo:warning={}  {}", prefix, entry.file_name().to_string_lossy());
                }
            }
        } else {
            println!("cargo:warning={} (file)", path.display());
        }
        Ok(())
    }
    // 根节点打印特殊处理
    println!("cargo:warning=Listing {}", root.display());
    for entry in fs::read_dir(root)? {
        let entry = entry?;
        let p = entry.path();
        if p.is_dir() {
            walk(&p, "")?;
        } else {
            println!("cargo:warning=  {}", entry.file_name().to_string_lossy());
        }
    }
    Ok(())
}

fn main() {
    std::env::set_var("RUST_BACKTRACE", "full");
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
