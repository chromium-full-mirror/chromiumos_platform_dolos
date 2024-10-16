# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Simple utility to update the dolos zephyr west.yml to point to the
# correct ti project and revision.

import yaml


with open("west.yml") as stream:
    original = yaml.safe_load(stream)

for project in original["manifest"]["projects"]:
    if project["name"] == "hal_ti":
        project["revision"] = "mspm0_dev"
        project["url"] = "https://github.com/msp-ti/hal_ti"

with open("west.yml", "w") as file:
    yaml.dump(original, file)
