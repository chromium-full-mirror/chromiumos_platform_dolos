#!/usr/bin/env bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <Path to CCS workspace>" >&2
  exit 1
fi

WS=$1
TI_PATH=$(readlink -f ~/ti/)
VERSION="0.$(git log --oneline | wc -l)-$(git describe --always --dirty)"
CONFIG=Debug

rm "${CONFIG}"/*.txt

echo "#define DOLOS_VERSION \"${VERSION}\"" > version.h
"${TI_PATH}"/ccs1250/ccs/eclipse/eclipse eclipse -noSplash -data "${WS}" -application com.ti.ccstudio.apps.projectBuild -ccs.projects dolos -ccs.configuration "${CONFIG}"

cp "${CONFIG}"/dolos.txt "${CONFIG}/dolos-$(git describe --always).txt"
