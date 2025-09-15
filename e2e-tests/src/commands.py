# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=missing-module-docstring
# pylint: disable=missing-function-docstring

import os
import time
import xmlrpc.client


PORT = 9999

SERVOD_HOST_MAIN = os.getenv("SERVOD_HOST_MAIN", "servod_main")
SERVOD_HOST_POWER = os.getenv("SERVOD_HOST_POWER", "servod_power")

servo_client_main = xmlrpc.client.ServerProxy(f"http://{SERVOD_HOST_MAIN}:{PORT}")
servo_client_power = xmlrpc.client.ServerProxy(f"http://{SERVOD_HOST_POWER}:{PORT}")


def dut_set_power(state: str):
    servo_client_main.set("servo_uart_cmd", f"cc {state}")


def dolos_set_power(state: str):
    servo_client_power.set("servo_uart_cmd", f"cc {state}")


def dolos_set_host_port_power(state: str):
    servo_client_main.set("top_usb_pwr_en", f"{state}")


def dolos_powercycle():
    dolos_set_power("off")
    dolos_set_host_port_power("off")
    time.sleep(1)
    dolos_set_host_port_power("on")
    dolos_set_power("on")
