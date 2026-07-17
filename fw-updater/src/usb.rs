// Copyright 2026 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use anyhow::{bail, Result};
use log::{debug, info, trace, warn};
use rusb::{Context, DeviceHandle, UsbContext};
use std::time::Duration;

use crate::bsl::Transport;

#[allow(dead_code)]
const DFU_REQUEST_DETACH: u8 = 0;
const DFU_REQUEST_DNLOAD: u8 = 1;
const DFU_REQUEST_UPLOAD: u8 = 2;
const DFU_REQUEST_GETSTATUS: u8 = 3;
const DFU_REQUEST_CLRSTATUS: u8 = 4;
#[allow(dead_code)]
const DFU_REQUEST_GETSTATE: u8 = 5;
const DFU_REQUEST_ABORT: u8 = 6;

const DFU_STATE_IDLE: u8 = 2;
const DFU_STATE_DNLOAD_IDLE: u8 = 5;
const DFU_STATE_ERROR: u8 = 10;

use std::path::{Path, PathBuf};

/// Represents the physical USB topology location (Hub Serial Number, Bus number, and Hub Port hierarchy).
/// Used to disambiguate multiple identical devices connected to the same host.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UsbLocation {
    pub bus: u8,
    pub ports: Vec<u8>,
    pub parent_hub_serial: Option<String>,
    pub port_on_parent: Option<u8>,
}

impl UsbLocation {
    /// Resolves the physical USB topology location for a given TTY character device.
    /// Traverses Linux sysfs (/sys/class/tty/<tty>) to extract bus, port path, and parent hub serial.
    pub fn from_tty_path(tty_path: &str) -> Option<Self> {
        let real_path = std::fs::canonicalize(tty_path).unwrap_or_else(|_| PathBuf::from(tty_path));
        let tty_name = real_path.file_name()?.to_str()?;

        let sysfs_path = Path::new("/sys/class/tty").join(tty_name);
        let canonical_sysfs = std::fs::canonicalize(&sysfs_path).ok()?;

        let mut curr = canonical_sysfs.as_path();
        let mut usb_dev_dir = None;
        while let Some(parent) = curr.parent() {
            if parent.join("busnum").exists() {
                usb_dev_dir = Some(parent);
                break;
            }
            curr = parent;
        }

        let usb_dev = usb_dev_dir?;
        Self::from_sysfs_dir(usb_dev)
    }

    /// Resolves the physical USB topology location from a sysfs USB device directory.
    pub fn from_sysfs_dir(usb_dev: &Path) -> Option<Self> {
        let canonical = std::fs::canonicalize(usb_dev).ok()?;
        let bus: u8 = std::fs::read_to_string(canonical.join("busnum")).ok()?.trim().parse().ok()?;

        let mut ports = Vec::new();
        if let Some(name) = canonical.file_name().and_then(|n| n.to_str()) {
            if let Some(idx) = name.find('-') {
                let port_part = &name[idx + 1..];
                ports = port_part
                    .split('.')
                    .filter_map(|s| s.parse::<u8>().ok())
                    .collect();
            }
        }

        if ports.is_empty() {
            if let Ok(devpath_str) = std::fs::read_to_string(canonical.join("devpath")) {
                ports = devpath_str
                    .trim()
                    .split('.')
                    .filter_map(|s| s.parse::<u8>().ok())
                    .collect();
            }
        }

        let port_on_parent = ports.last().copied();

        let mut parent_hub_serial = None;
        let mut ancestor = canonical.parent();
        while let Some(p) = ancestor {
            if p.join("busnum").exists() {
                if let Ok(s) = std::fs::read_to_string(p.join("serial")) {
                    let trimmed = s.trim().to_string();
                    if !trimmed.is_empty() {
                        parent_hub_serial = Some(trimmed);
                        break;
                    }
                }
            }
            ancestor = p.parent();
        }

        Some(UsbLocation {
            bus,
            ports,
            parent_hub_serial,
            port_on_parent,
        })
    }

    /// Resolves the physical USB topology location directly from a rusb::Device.
    pub fn from_rusb_device<C: UsbContext>(device: &rusb::Device<C>) -> Self {
        let bus = device.bus_number();
        let ports = device.port_numbers().unwrap_or_default();
        let port_on_parent = if !ports.is_empty() {
            ports.last().copied()
        } else {
            Some(device.port_number())
        };

        let mut parent_hub_serial = None;

        if !ports.is_empty() {
            let port_str = ports.iter().map(|p| p.to_string()).collect::<Vec<_>>().join(".");
            let sysfs_name = format!("{}-{}", bus, port_str);
            let sysfs_path = Path::new("/sys/bus/usb/devices").join(sysfs_name);
            if let Ok(canonical) = std::fs::canonicalize(&sysfs_path) {
                let mut ancestor = canonical.parent();
                while let Some(p) = ancestor {
                    if p.join("busnum").exists() {
                        if let Ok(s) = std::fs::read_to_string(p.join("serial")) {
                            let trimmed = s.trim().to_string();
                            if !trimmed.is_empty() {
                                parent_hub_serial = Some(trimmed);
                                break;
                            }
                        }
                    }
                    ancestor = p.parent();
                }
            }
        }

        UsbLocation {
            bus,
            ports,
            parent_hub_serial,
            port_on_parent,
        }
    }

    /// Checks if a candidate USB location matches the target location.
    pub fn matches(&self, cand: &UsbLocation) -> bool {
        // 1. If both have parent hub serial numbers, match by parent hub serial + port
        if let (Some(ref target_serial), Some(ref cand_serial)) = (&self.parent_hub_serial, &cand.parent_hub_serial) {
            if target_serial == cand_serial {
                if let (Some(target_port), Some(cand_port)) = (self.port_on_parent, cand.port_on_parent) {
                    if target_port == cand_port {
                        return true;
                    }
                } else {
                    return true;
                }
            }
        }

        // 2. Fallback: match by bus and exact port path hierarchy
        if self.bus == cand.bus && !self.ports.is_empty() && self.ports == cand.ports {
            return true;
        }

        // 3. Fallback: match by bus and port on parent
        if self.bus == cand.bus && self.port_on_parent.is_some() && self.port_on_parent == cand.port_on_parent {
            return true;
        }

        false
    }
}

pub struct Usb {
    handle: DeviceHandle<Context>,
    interface: u8,
    read_buffer: Vec<u8>,
    upload_block_num: u16,
}

impl Transport for Usb {
    fn write_all(&mut self, data: &[u8]) -> Result<()> {
        trace!("USB Write {} bytes", data.len());
        self.read_buffer.clear();
        self.upload_block_num = 0;

        let chunk_size = 512;
        let mut block_num = 0;
        let total_size = data.len();
        let mut last_percent = 0;

        for chunk in data.chunks(chunk_size) {
            self.dfu_dnload(block_num, chunk)?;
            self.wait_for_idle()?;

            if total_size > 1024 {
                let written = std::cmp::min((block_num as usize + 1) * chunk_size, total_size);
                let percent = (written as f32 / total_size as f32 * 100.0) as u32;
                if percent >= last_percent + 20 {
                    info!("{} %", percent);
                    last_percent = percent;
                }
            }

            block_num += 1;
        }

        // Send 0-length DFU_DNLOAD to terminate download phase
        self.dfu_dnload(block_num, &[])?;
        self.wait_for_idle()?;

        Ok(())
    }

    fn read_exact(&mut self, buf: &mut [u8]) -> Result<()> {
        trace!("USB Read {} bytes", buf.len());
        let mut needed = buf.len();
        let mut offset = 0;
        let mut retry_count = 0;

        while needed > 0 {
            if self.read_buffer.is_empty() {
                let mut temp_buf = vec![0u8; 512];
                let request_type = rusb::request_type(rusb::Direction::In, rusb::RequestType::Class, rusb::Recipient::Interface);
                let size = self.handle.read_control(request_type, DFU_REQUEST_UPLOAD, self.upload_block_num, self.interface as u16, &mut temp_buf, Duration::from_secs(2)).unwrap_or(0);

                if size == 0 {
                    retry_count += 1;
                    if retry_count > 50 {
                        bail!("DFU_UPLOAD returned 0 bytes repeatedly, needed {} more", needed);
                    }
                    std::thread::sleep(Duration::from_millis(10));
                    continue;
                }

                retry_count = 0;
                temp_buf.truncate(size);
                trace!("USB DFU_UPLOAD block {} read {} bytes", self.upload_block_num, size);
                self.upload_block_num += 1;
                self.read_buffer = temp_buf;
            }

            let take = std::cmp::min(needed, self.read_buffer.len());
            if self.read_buffer.len() > take {
                debug!("DFU upload buffer received {} bytes, consuming {}", self.read_buffer.len(), take);
            }
            buf[offset..offset + take].copy_from_slice(&self.read_buffer[..take]);
            self.read_buffer.drain(..take);
            offset += take;
            needed -= take;
        }

        Ok(())
    }

    fn flush_input(&mut self) -> Result<()> {
        self.read_buffer.clear();
        self.upload_block_num = 0;
        Ok(())
    }

    fn is_usb(&self) -> bool {
        true
    }
}

impl Usb {
    pub fn open(vid: u16, pid: u16, location: Option<&UsbLocation>) -> Result<Self> {
        if let Some(loc) = location {
            if let Some(ref hub_sn) = loc.parent_hub_serial {
                info!("Opening USB device {:04x}:{:04x} on parent hub {} (bus {} ports {:?})", vid, pid, hub_sn, loc.bus, loc.ports);
            } else {
                info!("Opening USB device {:04x}:{:04x} on bus {} ports {:?}", vid, pid, loc.bus, loc.ports);
            }
        } else {
            info!("Opening USB device {:04x}:{:04x}", vid, pid);
        }

        let start = std::time::Instant::now();
        let timeout = Duration::from_secs(15);
        let mut handle = None;

        while start.elapsed() < timeout {
            // Instantiate a fresh rusb Context on each poll to force libusb to
            // refresh its device list from Linux sysfs/udev.
            if let Ok(context) = Context::new() {
                if let Ok(devices) = context.devices() {
                    let mut matching_devices = Vec::new();
                    for device in devices.iter() {
                        if let Ok(desc) = device.device_descriptor() {
                            if desc.vendor_id() == vid && desc.product_id() == pid {
                                matching_devices.push(device);
                            }
                        }
                    }

                    let mut matching_target = Vec::new();
                    let mut loc_strs = Vec::new();

                    for device in matching_devices {
                        let cand_loc = UsbLocation::from_rusb_device(&device);
                        let hub_sn_str = cand_loc.parent_hub_serial.as_deref().unwrap_or("unknown");
                        loc_strs.push(format!("hub serial {} ({:?})", hub_sn_str, cand_loc));

                        if let Some(loc) = location {
                            if loc.matches(&cand_loc) {
                                matching_target.push((device, cand_loc));
                            }
                        } else {
                            matching_target.push((device, cand_loc));
                        }
                    }

                    if matching_target.len() > 1 {
                        bail!(
                            "Multiple USB DFU devices ({:04x}:{:04x}) detected at [{}]. Please specify target device via --hub-serial <SERIAL> (or --uart <PORT>) to disambiguate.",
                            vid, pid, loc_strs.join(", ")
                        );
                    }

                    for (device, cand_loc) in matching_target {
                        info!(
                            "Found matching USB DFU device {:04x}:{:04x} at bus {} dev {} location {:?}",
                            vid, pid, device.bus_number(), device.address(), cand_loc
                        );

                        match device.open() {
                            Ok(h) => {
                                info!(
                                    "Connected to USB DFU device {:04x}:{:04x} (location {:?})",
                                    vid, pid, cand_loc
                                );
                                handle = Some(h);
                                break;
                            }
                            Err(e) => {
                                warn!(
                                    "Found matching USB DFU device {:04x}:{:04x} at bus {} dev {}, but failed to open: {}",
                                    vid, pid, device.bus_number(), device.address(), e
                                );
                                if e == rusb::Error::Access {
                                    warn!("PERMISSION DENIED: Access to USB device {:04x}:{:04x} is restricted. Try running with sudo or adding a udev rule.", vid, pid);
                                }
                            }
                        }
                    }
                }
            }
            if handle.is_some() {
                break;
            }
            std::thread::sleep(Duration::from_millis(200));
        }

        let mut handle = handle.ok_or_else(|| {
            if let Some(loc) = location {
                anyhow::anyhow!(
                    "USB device {:04x}:{:04x} matching location {:?} not found or could not be opened within timeout",
                    vid, pid, loc
                )
            } else {
                anyhow::anyhow!("USB device {:04x}:{:04x} not found or could not be opened within timeout", vid, pid)
            }
        })?;

        let interface = 0;
        handle.set_auto_detach_kernel_driver(true).ok();
        let _ = handle.claim_interface(interface);

        let mut usb = Usb {
            handle,
            interface,
            read_buffer: Vec::new(),
            upload_block_num: 0,
        };

        usb.dfu_abort().ok(); // Clear any ongoing state
        usb.dfu_getstatus().ok(); // clear status

        Ok(usb)
    }

    fn dfu_dnload(&mut self, block_num: u16, data: &[u8]) -> Result<()> {
        let request_type = rusb::request_type(rusb::Direction::Out, rusb::RequestType::Class, rusb::Recipient::Interface);
        let size = self.handle.write_control(request_type, DFU_REQUEST_DNLOAD, block_num, self.interface as u16, data, Duration::from_secs(2))?;
        if size != data.len() {
            bail!("Failed to write all data to USB, wrote {} of {}", size, data.len());
        }
        Ok(())
    }

    fn dfu_getstatus(&mut self) -> Result<(u8, u8, u32)> {
        let mut buf = [0u8; 6];
        let request_type = rusb::request_type(rusb::Direction::In, rusb::RequestType::Class, rusb::Recipient::Interface);
        self.handle.read_control(request_type, DFU_REQUEST_GETSTATUS, 0, self.interface as u16, &mut buf, Duration::from_secs(2))?;
        let status = buf[0];
        let poll_timeout = u32::from_le_bytes([buf[1], buf[2], buf[3], 0]);
        let state = buf[4];
        trace!("DFU status: status={}, state={}, poll_timeout={}", status, state, poll_timeout);
        Ok((status, state, poll_timeout))
    }

    fn dfu_clrstatus(&mut self) -> Result<()> {
        let request_type = rusb::request_type(rusb::Direction::Out, rusb::RequestType::Class, rusb::Recipient::Interface);
        self.handle.write_control(request_type, DFU_REQUEST_CLRSTATUS, 0, self.interface as u16, &[], Duration::from_secs(2))?;
        Ok(())
    }

    fn dfu_abort(&mut self) -> Result<()> {
        let request_type = rusb::request_type(rusb::Direction::Out, rusb::RequestType::Class, rusb::Recipient::Interface);
        self.handle.write_control(request_type, DFU_REQUEST_ABORT, 0, self.interface as u16, &[], Duration::from_secs(2))?;
        Ok(())
    }

    fn wait_for_idle(&mut self) -> Result<()> {
        let start = std::time::Instant::now();
        let timeout = Duration::from_secs(5);
        while start.elapsed() < timeout {
            let (status, state, poll_timeout) = self.dfu_getstatus()?;
            if status != 0 {
                warn!("DFU Error status: {}, attempting clear status", status);
                self.dfu_clrstatus()?;
            }
            if state == DFU_STATE_IDLE || state == DFU_STATE_DNLOAD_IDLE {
                return Ok(());
            }
            if state == DFU_STATE_ERROR {
                self.dfu_clrstatus()?;
            }
            let wait_ms = std::cmp::max(poll_timeout as u64, 1);
            std::thread::sleep(Duration::from_millis(wait_ms));
        }
        bail!("Timeout waiting for DFU idle state");
    }
}
