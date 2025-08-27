#!/usr/bin/env bash
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(b/438110087) Remove legacy version major
if [[ -n "$BUILD_MODE" ]]; then
        VERSION_MAJOR="1"
else
        VERSION_MAJOR="0"
fi
VERSION_MINOR=$(git log --oneline | wc -l)
VERSION_PATCH="0"
VERSION_TWEAK="0"
EXTRAVERSION=$(git describe --always --dirty)


cd "$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )/.." || exit

echo "VERSION_MAJOR = ${VERSION_MAJOR}" > VERSION
{
        echo "VERSION_MINOR = ${VERSION_MINOR}"
        echo "PATCHLEVEL = ${VERSION_PATCH}"
        echo "VERSION_TWEAK = ${VERSION_TWEAK}"
        echo "EXTRAVERSION = ${EXTRAVERSION}"
} >> VERSION
