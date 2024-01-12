// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::Result;
use std::env;
use std::fs;

const PKG_VERSION: &str = env!("CARGO_PKG_VERSION");

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
    // Try to read ebuild generated SHA from build directory;
    // if it does not exists use git command to get SHA.
    let version = match fs::read_to_string("./sha.txt") {
        Ok(val) => match val.as_str().trim() {
            "" => "local".to_string(),
            _ => val,
        },
        Err(_) => get_git_commit_id(),
    };

    // Make cargo to rerun if CARGO_PKG_VERSION is changed.
    println!("cargo:rerun-if-env-changed=CARGO_PKG_VERSION");

    let dt = chrono::offset::Utc::now();
    println!(
        "cargo:rustc-env=VERSION=v{PKG_VERSION}-{} {}",
        version,
        dt.format("%Y-%m-%dT%H:%M:%S")
    );
}

fn main() -> Result<()> {
    print_version();
    Ok(())
}
