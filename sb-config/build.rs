// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern crate bindgen;

use anyhow::Result;
use bindgen::callbacks::ParseCallbacks;
use std::env;
use std::fmt::{Debug, Formatter};
use std::path::{Path, PathBuf};

const PKG_VERSION: &str = env!("CARGO_PKG_VERSION");

/// Callback structure to derive zerocopy::FromBytes and AsBytes on all c structures.
struct BindGenParseCallBack;

impl Debug for BindGenParseCallBack {
    fn fmt(&self, _: &mut Formatter<'_>) -> std::fmt::Result {
        Ok(())
    }
}

impl ParseCallbacks for BindGenParseCallBack {
    fn add_derives(&self, _name: &str) -> Vec<String> {
        vec![
            "FromBytes".to_string(),
            "FromZeroes".to_string(),
            "AsBytes".to_string(),
        ]
    }
}

/// Returns current username.
fn get_user_name() -> String {
    env::var("USER").expect("current username not available")
}

/// Returns git local commit id(SHA)
///
/// Runs "git log" and "git describe" commands to find the number
/// of commits and last SHA.
fn get_git_commit_id() -> String {
    use std::process::Command;

    let git_log_output = Command::new("git")
        .args(["log", "--oneline"])
        .output()
        .expect("failed to run 'git log'");
    let commit_count = String::from_utf8(git_log_output.stdout)
        .unwrap()
        .matches('\n')
        .count();
    let git_desc_output = Command::new("git")
        .args(["describe", "--always", "--dirty"])
        .output()
        .expect("failed to run 'git describe'");

    format!(
        "{}-{} {}",
        commit_count,
        String::from_utf8(git_desc_output.stdout).unwrap().trim(),
        get_user_name()
    )
}

/// Print git SHA as cargo build env variable.
fn print_version() {
    let version = get_git_commit_id();

    // Make cargo to rerun if CARGO_PKG_VERSION is changed.
    println!("cargo:rerun-if-env-changed=CARGO_PKG_VERSION");

    let dt = chrono::offset::Utc::now();
    println!(
        "cargo:rustc-env=VERSION=v{PKG_VERSION}-{} {}",
        version,
        dt.format("%Y-%m-%dT%H:%M:%S")
    );
}

fn generate_rs_from_header(c_header: &Path, rust_file_name: &Path) {
    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed={}", c_header.display());
    let bindings = bindgen::Builder::default()
        .header(c_header.display().to_string())
        .prepend_enum_name(false)
        .layout_tests(false)
        // Tell cargo to invalidate the built crate whenever any of the included header changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // To add `#[derive(FromBytes, AsBytes)]` to all structs
        .parse_callbacks(Box::new(BindGenParseCallBack))
        .derive_default(true)
        .derive_eq(true)
        .derive_partialeq(true)
        .derive_ord(true)
        .derive_partialord(true)
        .derive_hash(true)
        .newtype_enum("*")
        .generate()
        .unwrap_or_else(|_| panic!("Unable to generate bindings for {}", c_header.display()));

    // Write the bindings to the $OUT_DIR/rust_file_name.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join(rust_file_name))
        .unwrap_or_else(|_| panic!("Couldn't write bindings for {}", rust_file_name.display()));
}

fn main() -> Result<()> {
    print_version();
    generate_rs_from_header(
        Path::new("src/eeprom_layout.h"),
        Path::new("eeprom_layout.rs"),
    );
    Ok(())
}
