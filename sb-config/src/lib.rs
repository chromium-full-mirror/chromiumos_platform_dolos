// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub mod eeprom_layout {
    #![allow(non_upper_case_globals, non_camel_case_types, dead_code)]
    use zerocopy_derive::{AsBytes, FromBytes, FromZeroes};
    include!(concat!(env!("OUT_DIR"), "/eeprom_layout.rs"));
}
