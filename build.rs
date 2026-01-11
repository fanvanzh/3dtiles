extern crate cmake;
use std::{env, fs, io, path::Path};

use cmake::Config;

// Copy CMake's compile_commands.json into a stable workspace path for editors.
fn export_compile_commands(out_dir: &Path) {
    let cmake_build_dir = out_dir.join("build");
    let src = cmake_build_dir.join("compile_commands.json");
    if !src.exists() {
        println!("cargo:warning=compile_commands.json not found at {}", src.display());
        return;
    }

    let dst_dir = Path::new(&env::var("CARGO_MANIFEST_DIR").unwrap()).join("build");
    if let Err(err) = fs::create_dir_all(&dst_dir) {
        println!("cargo:warning=failed to create build dir {}: {}", dst_dir.display(), err);
        return;
    }

    let dst = dst_dir.join("compile_commands.json");
    match fs::copy(&src, &dst) {
        Ok(_) => println!("cargo:warning=exported compile_commands.json to {}", dst.display()),
        Err(err) => println!(
            "cargo:warning=failed to copy compile_commands.json to {}: {}",
            dst.display(),
            err
        ),
    }
}

fn create_vcpkg_installed_dir_symlink() -> std::io::Result<()> {
    let cargo_manifest_dir = std::env::var("CARGO_MANIFEST_DIR")
        .expect("Failed to get CARGO_MANIFEST_DIR environment variable");
    let root_vcpkg_installed_dir = Path::new(&cargo_manifest_dir).join("vcpkg_installed");
    if root_vcpkg_installed_dir.exists() {
        let out_dir = env::var("OUT_DIR").unwrap();
        let build_vcpkg_installed_dir = Path::new(&out_dir).join("build").join("vcpkg_installed");

        // ensure all parent dirs exists
        let parent_build_vcpkg_installed_dir = build_vcpkg_installed_dir.parent().expect(&format!("there is no parent directory in {}", build_vcpkg_installed_dir.display()));
        fs::create_dir_all(parent_build_vcpkg_installed_dir).expect("Failed to create build dir");

        println!("cargo:warning=build_vcpkg_installed_dir: {}", build_vcpkg_installed_dir.display());

        if build_vcpkg_installed_dir.exists() {
            println!(
                "cargo:warning=build_vcpkg_installed_dir already exists, so there is no need to create it again.: {:?}",
                build_vcpkg_installed_dir
            );
            return Ok(());
        }

        #[cfg(target_family = "unix")]
        {
            std::os::unix::fs::symlink(&root_vcpkg_installed_dir, &build_vcpkg_installed_dir)
            .expect(&format!("Failed to create symlink for Unix-like os from {} -> {}", root_vcpkg_installed_dir.display(), build_vcpkg_installed_dir.display()));
        }

        #[cfg(target_family = "windows")]
        {
            std::os::windows::fs::symlink_dir(&root_vcpkg_installed_dir, &build_vcpkg_installed_dir)
                .expect(&format!("Failed to create symlink for windows os from {} -> {}", root_vcpkg_installed_dir.display(), build_vcpkg_installed_dir.display()));
        }
    }

    Ok(())
}

fn build_win_msvc() {
    // Get VCPKG_ROOT environment variable
    let vcpkg_root = std::env::var("VCPKG_ROOT").expect("VCPKG_ROOT environment variable is not set");

    let vcpkg_has_been_installed = env::var("VCPKG_HAS_BEEN_INSTALLED").unwrap_or_default() == "1";
    if vcpkg_has_been_installed {
        create_vcpkg_installed_dir_symlink().expect("create_dir_symlink fail");
    }

    // Check if strict mode is enabled via environment variable
    let enable_strict = env::var("ENABLE_STRICT_CHECKS").unwrap_or_default() == "1";

    let mut config = Config::new(".");
    config
        .define("CMAKE_TOOLCHAIN_FILE", format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root))
        .define("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
        .very_verbose(true);

    if enable_strict {
        println!("cargo:warning=Building with STRICT CHECKS enabled (CI mode)");
        config.define("ENABLE_STRICT_CHECKS", "ON");
    }

    let dst = config.build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    // for FFI C++ static library
    println!("cargo:rustc-link-lib=static=_3dtile");

    // Link Search Path for Draco library
    let source_dir = env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR environment variable is not set");
    // Link Search Path for Draco library
    println!("cargo:rustc-link-search=native={}/thirdparty/draco", Path::new(&source_dir).display());
    println!("cargo:rustc-link-lib=static=draco");

    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:warning=out_dir = {}", &out_dir);
    export_compile_commands(Path::new(&out_dir));
    print_vcpkg_tree(Path::new(&out_dir)).unwrap();
    // vcpkg_installed path
    let vcpkg_installed_dir = Path::new(&out_dir)
        .join("build")
        .join("vcpkg_installed")
        .join("x64-windows");

    // Link Search Path for third party library
    let vcpkg_installed_lib_dir = vcpkg_installed_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_lib_dir.display());

    // 1. FFI static
    println!("cargo:rustc-link-lib=static=_3dtile");

    // 2. OSG
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osg");
    let osg_plugins_lib = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    println!("cargo:rustc-link-search=native={}", osg_plugins_lib.display());

    // 3. OpenThreads (依赖 _3dtile)
    println!("cargo:rustc-link-lib=OpenThreads");

    // 4. GDAL dependencies
    println!("cargo:rustc-link-lib=gdal");
    println!("cargo:rustc-link-lib=basisu_encoder");
    println!("cargo:rustc-link-lib=meshoptimizer");
    // zstd is required by gdal/basisu when building KTX2 (supercompression) and some GDAL drivers
    println!("cargo:rustc-link-lib=zstd");

    // 5. sqlite
    println!("cargo:rustc-link-lib=sqlite3");

    // copy gdal and proj data
    let vcpkg_share_dir = vcpkg_installed_dir.join("share");
    copy_gdal_data(vcpkg_share_dir.to_str().unwrap());
    copy_proj_data(vcpkg_share_dir.to_str().unwrap());

    // copy OSG plugins
    let osg_plugins_src = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    copy_osg_plugins(osg_plugins_src.to_str().unwrap());
}

fn get_target_dir() -> std::path::PathBuf {
    let profile = env::var("PROFILE").unwrap();
    let target_dir =
        Path::new(&env::var("CARGO_TARGET_DIR").unwrap_or("target".into())).join(&profile);
    target_dir
}

fn build_linux_unknown() {
    let vcpkg_root = std::env::var("VCPKG_ROOT").expect("VCPKG_ROOT environment variable is not set");

    let vcpkg_has_been_installed = env::var("VCPKG_HAS_BEEN_INSTALLED").unwrap_or_default() == "1";
    if vcpkg_has_been_installed {
        create_vcpkg_installed_dir_symlink().expect("create_dir_symlink fail");
    }

    // Check if strict mode is enabled via environment variable
    let enable_strict = env::var("ENABLE_STRICT_CHECKS").unwrap_or_default() == "1";

    // Get VCPKG_ROOT environment variable
    let mut config = Config::new(".");
    config
        .define("CMAKE_TOOLCHAIN_FILE",format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root))
        .define("CMAKE_C_COMPILER", "/usr/bin/gcc")
        .define("CMAKE_CXX_COMPILER", "/usr/bin/g++")
        .define("CMAKE_MAKE_PROGRAM", "/usr/bin/make")
        .define("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
        .very_verbose(true);

    if enable_strict {
        println!("cargo:warning=Building with STRICT CHECKS enabled (CI mode)");
        config.define("ENABLE_STRICT_CHECKS", "ON");
    }

    let dst = config.build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    // Link Search Path for Draco library
    let source_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    // Link Search Path for Draco library
    println!("cargo:rustc-link-search=native={}/thirdparty/draco", Path::new(&source_dir).display());
    println!("cargo:rustc-link-lib=static=draco");

    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:warning=out_dir = {}", &out_dir);
    export_compile_commands(Path::new(&out_dir));
    // print_vcpkg_tree(Path::new(&out_dir)).unwrap();
    // vcpkg_installed path
    let vcpkg_installed_dir = Path::new(&out_dir)
        .join("build")
        .join("vcpkg_installed")
        .join("x64-linux");
    // Link Search Path for third party library
    let vcpkg_installed_lib_dir = vcpkg_installed_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_lib_dir.display());
    let osg_plugins_lib = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    println!("cargo:rustc-link-search=native={}", osg_plugins_lib.display());

    // 0. System C++ library first
    println!("cargo:rustc-link-lib=stdc++");
    println!("cargo:rustc-link-lib=z");

    // 1. FFI static
    println!("cargo:rustc-link-lib=static=_3dtile");

    // 2. OSG
    // println!("cargo:rustc-link-lib=osgdb_jp2");
    println!("cargo:rustc-link-lib=GL");
    println!("cargo:rustc-link-lib=X11");
    println!("cargo:rustc-link-lib=Xi");
    println!("cargo:rustc-link-lib=Xrandr");
    println!("cargo:rustc-link-lib=dl");
    println!("cargo:rustc-link-lib=pthread");

    // Note: On Linux, OSG plugins are static libraries (.a) that must be linked at build time
    println!("cargo:rustc-link-lib=osgdb_jpeg");
    println!("cargo:rustc-link-lib=osgdb_osg");
    println!("cargo:rustc-link-lib=osgdb_serializers_osg");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osg");

    // 3. OpenThreads (依赖 _3dtile)
    println!("cargo:rustc-link-lib=OpenThreads");

    // 4. GDAL dependencies
    println!("cargo:rustc-link-lib=gdal");
    println!("cargo:rustc-link-lib=geos_c");
    println!("cargo:rustc-link-lib=geos");
    println!("cargo:rustc-link-lib=proj");
    println!("cargo:rustc-link-lib=sqlite3");
    println!("cargo:rustc-link-lib=expat");
    println!("cargo:rustc-link-lib=curl");
    println!("cargo:rustc-link-lib=ssl");
    println!("cargo:rustc-link-lib=crypto");
    println!("cargo:rustc-link-lib=uriparser");
    println!("cargo:rustc-link-lib=kmlbase");
    println!("cargo:rustc-link-lib=kmlengine");
    println!("cargo:rustc-link-lib=kmldom");
    println!("cargo:rustc-link-lib=kmlconvenience");
    // println!("cargo:rustc-link-lib=qhullcpp");
    println!("cargo:rustc-link-lib=Lerc");

    // 5. Other dependencies
    // println!("cargo:rustc-link-lib=hdf5_hl");
    // println!("cargo:rustc-link-lib=hdf5");
    println!("cargo:rustc-link-lib=json-c");
    // println!("cargo:rustc-link-lib=netcdf");
    println!("cargo:rustc-link-lib=sharpyuv");

    // 6. Image / compression libraries
    println!("cargo:rustc-link-lib=geotiff");
    println!("cargo:rustc-link-lib=gif");
    println!("cargo:rustc-link-lib=jpeg");
    println!("cargo:rustc-link-lib=png");
    println!("cargo:rustc-link-lib=tiff");
    println!("cargo:rustc-link-lib=webp");
    println!("cargo:rustc-link-lib=xml2");
    println!("cargo:rustc-link-lib=lzma");
    println!("cargo:rustc-link-lib=openjp2");
    println!("cargo:rustc-link-lib=qhullstatic_r");
    println!("cargo:rustc-link-lib=minizip");
    println!("cargo:rustc-link-lib=spatialite");
    println!("cargo:rustc-link-lib=freexl");
    println!("cargo:rustc-link-lib=basisu_encoder");
    println!("cargo:rustc-link-lib=meshoptimizer");
    // zstd is required by gdal/basisu when building KTX2 (supercompression) and some GDAL drivers
    println!("cargo:rustc-link-lib=zstd");

    let vcpkg_share_dir = vcpkg_installed_dir.join("share");
    copy_gdal_data(vcpkg_share_dir.to_str().unwrap());
    copy_proj_data(vcpkg_share_dir.to_str().unwrap());

    // copy OSG plugins
    let osg_plugins_src = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    copy_osg_plugins(osg_plugins_src.to_str().unwrap());
}

fn build_macos() {
    let vcpkg_root = std::env::var("VCPKG_ROOT").expect("VCPKG_ROOT environment variable is not set");

    let vcpkg_has_been_installed = env::var("VCPKG_HAS_BEEN_INSTALLED").unwrap_or_default() == "1";
    if vcpkg_has_been_installed {
        create_vcpkg_installed_dir_symlink().expect("create_dir_symlink fail");
    }

    // Check if strict mode is enabled via environment variable
    let enable_strict = env::var("ENABLE_STRICT_CHECKS").unwrap_or_default() == "1";

    // Get VCPKG_ROOT environment variable
    let mut config = Config::new(".");
    config
        .define("CMAKE_TOOLCHAIN_FILE",format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root))
        .define("VCPKG_INSTALL_OPTIONS", "--allow-unsupported")
        .define("CMAKE_C_COMPILER", "/usr/bin/clang")
        .define("CMAKE_CXX_COMPILER", "/usr/bin/clang++")
        .define("CMAKE_MAKE_PROGRAM", "/usr/bin/make")
        .define("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
        .very_verbose(true);

    if enable_strict {
        println!("cargo:warning=Building with STRICT CHECKS enabled (CI mode)");
        config.define("ENABLE_STRICT_CHECKS", "ON");
    }

    let dst = config.build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    let source_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    // Link Search Path for Draco library
    println!("cargo:rustc-link-search=native={}/thirdparty/draco", Path::new(&source_dir).display());
    println!("cargo:rustc-link-lib=static=draco");

    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:warning=out_dir = {}", &out_dir);
    export_compile_commands(Path::new(&out_dir));
    // print_vcpkg_tree(Path::new(&out_dir)).unwrap();
    // vcpkg_installed path
    let vcpkg_installed_dir = Path::new(&out_dir)
        .join("build")
        .join("vcpkg_installed")
        .join("arm64-osx");
    // Link Search Path for third party library
    let vcpkg_installed_lib_dir = vcpkg_installed_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", vcpkg_installed_lib_dir.display());
    let osg_plugins_lib = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    println!("cargo:rustc-link-search=native={}", osg_plugins_lib.display());

    // 0. System C++ library first
    println!("cargo:rustc-link-lib=c++");
    println!("cargo:rustc-link-lib=z");

    // 1. FFI static
    println!("cargo:rustc-link-lib=static=_3dtile");

    // 2. OSG
    // Note: On macOS ARM64, OSG plugins are static libraries (.a) that must be linked at build time
    println!("cargo:rustc-link-lib=osgdb_jpeg");
    println!("cargo:rustc-link-lib=osgdb_osg");
    println!("cargo:rustc-link-lib=osgdb_serializers_osg");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osg");

    // 3. OpenThreads (依赖 _3dtile)
    println!("cargo:rustc-link-lib=OpenThreads");

    // 4. GDAL dependencies
    println!("cargo:rustc-link-lib=gdal");
    println!("cargo:rustc-link-lib=geos_c");
    println!("cargo:rustc-link-lib=geos");
    println!("cargo:rustc-link-lib=proj");
    println!("cargo:rustc-link-lib=sqlite3");
    println!("cargo:rustc-link-lib=expat");
    println!("cargo:rustc-link-lib=curl");
    println!("cargo:rustc-link-lib=ssl");
    println!("cargo:rustc-link-lib=crypto");
    println!("cargo:rustc-link-lib=kmlbase");
    println!("cargo:rustc-link-lib=kmlengine");
    println!("cargo:rustc-link-lib=kmldom");
    println!("cargo:rustc-link-lib=kmlconvenience");
    // println!("cargo:rustc-link-lib=qhullcpp");
    println!("cargo:rustc-link-lib=Lerc");

    // 5. Other dependencies
    // println!("cargo:rustc-link-lib=hdf5_hl");
    // println!("cargo:rustc-link-lib=hdf5");
    println!("cargo:rustc-link-lib=json-c");
    // println!("cargo:rustc-link-lib=netcdf");
    println!("cargo:rustc-link-lib=sharpyuv");

    // 6. Image / compression libraries
    println!("cargo:rustc-link-lib=geotiff");
    println!("cargo:rustc-link-lib=gif");
    println!("cargo:rustc-link-lib=jpeg");
    println!("cargo:rustc-link-lib=png");
    println!("cargo:rustc-link-lib=tiff");
    println!("cargo:rustc-link-lib=webp");
    println!("cargo:rustc-link-lib=xml2");
    println!("cargo:rustc-link-lib=lzma");
    println!("cargo:rustc-link-lib=openjp2");
    println!("cargo:rustc-link-lib=qhullstatic_r");
    println!("cargo:rustc-link-lib=minizip");
    println!("cargo:rustc-link-lib=spatialite");
    println!("cargo:rustc-link-lib=freexl");
    println!("cargo:rustc-link-lib=basisu_encoder");
    println!("cargo:rustc-link-lib=meshoptimizer");
    // zstd is required by gdal/basisu when building KTX2 (supercompression) and some GDAL drivers
    println!("cargo:rustc-link-lib=zstd");

    // 7. System libraries / frameworks
    println!("cargo:rustc-link-lib=c++");
    println!("cargo:rustc-link-lib=z");
    println!("cargo:rustc-link-lib=framework=Security");
    println!("cargo:rustc-link-lib=framework=CoreFoundation");
    println!("cargo:rustc-link-lib=framework=Foundation");
    println!("cargo:rustc-link-lib=framework=OpenGL");
    println!("cargo:rustc-link-lib=framework=AppKit");
    println!("cargo:rustc-link-lib=framework=SystemConfiguration");

    // -----------------------------
    // Additional linker flags for Objective-C / Boost symbols in static libs
    // -----------------------------
    println!("cargo:rustc-link-arg=-ObjC");
    println!("cargo:rustc-link-arg=-all_load");

    let vcpkg_share_dir = vcpkg_installed_dir.join("share");
    copy_gdal_data(vcpkg_share_dir.to_str().unwrap());
    copy_proj_data(vcpkg_share_dir.to_str().unwrap());

    // copy OSG plugins for macOS x86_64
    let osg_plugins_src = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    copy_osg_plugins(osg_plugins_src.to_str().unwrap());
}

fn build_macos_x86_64() {
    let vcpkg_root = std::env::var("VCPKG_ROOT").expect("VCPKG_ROOT environment variable is not set");

    let vcpkg_has_been_installed = env::var("VCPKG_HAS_BEEN_INSTALLED").unwrap_or_default() == "1";
    if vcpkg_has_been_installed {
        create_vcpkg_installed_dir_symlink().expect("create_dir_symlink fail");
    }

    // Check if strict mode is enabled via environment variable
    let enable_strict = env::var("ENABLE_STRICT_CHECKS").unwrap_or_default() == "1";

    // Get VCPKG_ROOT environment variable
    let mut config = Config::new(".");
    config
        .define("CMAKE_TOOLCHAIN_FILE",format!("{}/scripts/buildsystems/vcpkg.cmake", vcpkg_root))
        .define("CMAKE_C_COMPILER", "/usr/bin/clang")
        .define("CMAKE_CXX_COMPILER", "/usr/bin/clang++")
        .define("CMAKE_MAKE_PROGRAM", "/usr/bin/make")
        .define("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
        .very_verbose(true);

    if enable_strict {
        println!("cargo:warning=Building with STRICT CHECKS enabled (CI mode)");
        config.define("ENABLE_STRICT_CHECKS", "ON");
    }

    let dst = config.build();
    println!("cmake dst = {}", dst.display());
    // Link Search Path for C++ Implementation
    println!("cargo:rustc-link-search=native={}/lib", dst.display());

    // Link Search Path for Draco library
    let source_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    // Link Search Path for Draco library
    println!("cargo:rustc-link-search=native={}/thirdparty/draco", Path::new(&source_dir).display());
    println!("cargo:rustc-link-lib=static=draco");

    let out_dir = env::var("OUT_DIR").unwrap();
    println!("cargo:warning=out_dir = {}", &out_dir);
    export_compile_commands(Path::new(&out_dir));
    // print_vcpkg_tree(Path::new(&out_dir)).unwrap();
    // vcpkg_installed path
    let vcpkg_installed_dir = Path::new(&out_dir)
        .join("build")
        .join("vcpkg_installed")
        .join("x64-osx");
    // Link Search Path for third party library
    let vcpkg_installed_lib_dir = vcpkg_installed_dir.join("lib");
    println!( "cargo:rustc-link-search=native={}", vcpkg_installed_lib_dir.display());
    let osg_plugins_lib = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    println!("cargo:rustc-link-search=native={}", osg_plugins_lib.display());

    // 0. System C++ library first
    println!("cargo:rustc-link-lib=c++");
    println!("cargo:rustc-link-lib=z");

    // 1. FFI static
    println!("cargo:rustc-link-lib=static=_3dtile");

    // 2. OSG
    // Note: On macOS x86_64, OSG plugins are static libraries (.a) that must be linked at build time
    println!("cargo:rustc-link-lib=osgdb_jpeg");
    println!("cargo:rustc-link-lib=osgdb_osg");
    println!("cargo:rustc-link-lib=osgdb_serializers_osg");
    println!("cargo:rustc-link-lib=osgUtil");
    println!("cargo:rustc-link-lib=osgDB");
    println!("cargo:rustc-link-lib=osg");

    // 3. OpenThreads (依赖 _3dtile)
    println!("cargo:rustc-link-lib=OpenThreads");

    // 4. GDAL dependencies
    println!("cargo:rustc-link-lib=gdal");
    println!("cargo:rustc-link-lib=geos_c");
    println!("cargo:rustc-link-lib=geos");
    println!("cargo:rustc-link-lib=proj");
    println!("cargo:rustc-link-lib=sqlite3");
    println!("cargo:rustc-link-lib=expat");
    println!("cargo:rustc-link-lib=curl");
    println!("cargo:rustc-link-lib=ssl");
    println!("cargo:rustc-link-lib=crypto");
    println!("cargo:rustc-link-lib=kmlbase");
    println!("cargo:rustc-link-lib=kmlengine");
    println!("cargo:rustc-link-lib=kmldom");
    println!("cargo:rustc-link-lib=kmlconvenience");
    // println!("cargo:rustc-link-lib=qhullcpp");
    println!("cargo:rustc-link-lib=Lerc");

    // 5. Other dependencies
    // println!("cargo:rustc-link-lib=hdf5_hl");
    // println!("cargo:rustc-link-lib=hdf5");
    println!("cargo:rustc-link-lib=json-c");
    // println!("cargo:rustc-link-lib=netcdf");
    println!("cargo:rustc-link-lib=sharpyuv");

    // 6. Image / compression libraries
    println!("cargo:rustc-link-lib=geotiff");
    println!("cargo:rustc-link-lib=gif");
    println!("cargo:rustc-link-lib=jpeg");
    println!("cargo:rustc-link-lib=png");
    println!("cargo:rustc-link-lib=tiff");
    println!("cargo:rustc-link-lib=webp");
    println!("cargo:rustc-link-lib=xml2");
    println!("cargo:rustc-link-lib=lzma");
    println!("cargo:rustc-link-lib=openjp2");
    println!("cargo:rustc-link-lib=qhullstatic_r");
    println!("cargo:rustc-link-lib=minizip");
    println!("cargo:rustc-link-lib=spatialite");
    println!("cargo:rustc-link-lib=freexl");
    println!("cargo:rustc-link-lib=basisu_encoder");
    println!("cargo:rustc-link-lib=meshoptimizer");
    // zstd is required by gdal/basisu when building KTX2 (supercompression) and some GDAL drivers
    println!("cargo:rustc-link-lib=zstd");

    // 7. System libraries / frameworks
    println!("cargo:rustc-link-lib=c++");
    println!("cargo:rustc-link-lib=z");
    println!("cargo:rustc-link-lib=framework=Security");
    println!("cargo:rustc-link-lib=framework=CoreFoundation");
    println!("cargo:rustc-link-lib=framework=Foundation");
    println!("cargo:rustc-link-lib=framework=OpenGL");
    println!("cargo:rustc-link-lib=framework=AppKit");
    println!("cargo:rustc-link-lib=framework=SystemConfiguration");

    // -----------------------------
    // Additional linker flags for Objective-C / Boost symbols in static libs
    // -----------------------------
    println!("cargo:rustc-link-arg=-ObjC");
    println!("cargo:rustc-link-arg=-all_load");

    let vcpkg_share_dir = vcpkg_installed_dir.join("share");
    copy_gdal_data(vcpkg_share_dir.to_str().unwrap());
    copy_proj_data(vcpkg_share_dir.to_str().unwrap());

    // copy OSG plugins for macOS x86_64
    let osg_plugins_src = vcpkg_installed_lib_dir.join("osgPlugins-3.6.5");
    copy_osg_plugins(osg_plugins_src.to_str().unwrap());
}

fn copy_gdal_data(share: &str) {
    let gdal_data = Path::new(share).join("gdal");
    let out_dir = get_target_dir().join("gdal");

    println!(
        "gdal_data -> {}, out_dir -> {}",
        gdal_data.display(),
        out_dir.display()
    );
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
    let out_dir = get_target_dir().join("proj");
    println!(
        "proj_data -> {}, out_dir -> {}",
        proj_data.display(),
        out_dir.display()
    );

    copy_dir_recursive(&proj_data, &out_dir).unwrap();
}

fn copy_osg_plugins(plugins_dir: &str) {
    let plugins_src = Path::new(plugins_dir);
    let out_dir = get_target_dir().join("osgPlugins-3.6.5");

    println!(
        "osg_plugins -> {}, out_dir -> {}",
        plugins_src.display(),
        out_dir.display()
    );

    if plugins_src.exists() {
        copy_dir_recursive(&plugins_src, &out_dir).unwrap();
    } else {
        println!(
            "cargo:warning=OSG plugins directory not found: {}",
            plugins_dir
        );
    }
}

/// 打印 ./vcpkg_installed 下的目录树，用 cargo:warning 输出，这样能在 cargo build 输出中看到
#[allow(dead_code)]
fn print_vcpkg_tree(root: &Path) -> io::Result<()> {
    if !root.exists() {
        println!("cargo:warning=path '{}' does not exist", root.display());
        return Ok(());
    }
    fn walk(path: &Path, prefix: &str) -> io::Result<()> {
        let meta = fs::metadata(path)?;
        if meta.is_dir() {
            println!(
                "cargo:warning={}{}",
                prefix,
                path.file_name()
                    .map(|s| s.to_string_lossy())
                    .unwrap_or_else(|| path.display().to_string().into())
            );
            let mut entries: Vec<_> = fs::read_dir(path)?.collect();
            entries.sort_by_key(|e| e.as_ref().map(|e| e.file_name()).ok());
            for entry in entries {
                let entry = entry?;
                let p = entry.path();
                if p.is_dir() {
                    walk(&p, &format!("{}  ", prefix))?;
                } else {
                    println!(
                        "cargo:warning={}  {}",
                        prefix,
                        entry.file_name().to_string_lossy()
                    );
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
            "x86_64-unknown-linux-gnu" => build_linux_unknown(),
            "x86_64-pc-windows-msvc" => build_win_msvc(),
            "aarch64-apple-darwin" => build_macos(),
            "x86_64-apple-darwin" => build_macos_x86_64(),
            &_ => {}
        },
        _ => {}
    }
}
