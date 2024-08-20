#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build the Dolos python code into an distribution package."""

import os
import subprocess
import sys

from setuptools import setup
from setuptools.command import build_py


def generate_proto(source):
    """Invokes the Protocol Compiler to generate a _pb2.py from the given
    .proto file.  Does nothing if the output already exists and is newer than
    the input."""

    output = source.replace(".proto", "_pb2.py")

    if not os.path.exists(output) or (
        os.path.exists(source) and os.path.getmtime(source) > os.path.getmtime(output)
    ):
        print(f"Generating {output}...")

        if not os.path.exists(source):
            sys.stderr.write(f"Can't find required file: {source}\n")
            sys.exit(-1)

        protoc_command = [
            "python3",
            "-m",
            "grpc_tools.protoc",
            "-I=.",
            "--python_out=.",
            source,
        ]
        if subprocess.call(protoc_command) != 0:
            sys.exit(-1)


proto_src = ["proto/doloscmd.proto"]


class DolosBuildPy(build_py.build_py):
    """Generate the python proto files for the install package."""

    def run(self):
        for file in proto_src:
            generate_proto(file)
        build_py.build_py.run(self)


# Chroot's copy of Python includes 'grpc_tools' but lacks 'pip',
# this means we can not include packages in it's 'setup_requires'.
# Unfortunately, setup.py requires it to access the 'grpc_tools' package
# when generating the protobufs.
setup_requires = None
if os.getenv("VIRTUAL_ENV"):
    setup_requires = ["grpcio-tools"]

setup(
    name="doloscmd",
    version="0.1",
    maintainer="chromium os",
    maintainer_email="chromium-os-dev@chromium.org",
    license="Chromium",
    package_dir={"doloscmd": "../doloscmd"},
    packages=[
        "doloscmd",
        "doloscmd.proto",
    ],
    setup_requires=setup_requires,
    cmdclass={"build_py": DolosBuildPy},
    entry_points={
        "console_scripts": ["doloscmd=doloscmd.doloscmd:main"],
    },
    description="Dolos command tools.",
)
