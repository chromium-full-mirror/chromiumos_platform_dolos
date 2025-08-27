These dockerfiles and instructions allow you to build the dolos firmware and
firmware updater without any special setup or tooling install.

This guide assumes you have docker installed on you machine and that you have
access to third_party/dolos.

Note these dockerfiles do not work on checkouts that have been created by repo
as they use git commands.  .git directories for repo checkouts are symlinks and
the result of the symlink is not available to the docker build.

Create a directory to checkout the dolos source

```shell
mkdir dolos_source
cd dolos_source
```

Get the source code

```shell
git clone https://chromium.googlesource.com/chromiumos/platform/dolos
cd dolos
```

Please also configure your git hooks, these are obligatory to use in this repo:
```shell
pip install pre-commit --break-system-packages
pre-commit install
```

To upload your changes to gerrit you can use:
```shell
git push origin HEAD:refs/for/main
```

Build the docker image used to compile firmware. This may take some time, please
be patient. It is only needed to call this once, NOT everytime the build is
done.
```shell
docker build -f dockerfiles/Dockerfile.firmware -t dolos .
```

To build the legacy firmware, execute command from the root of the dolos repository:
```shell
docker run --rm --user $(id -u):$(id -g) -v `pwd`:/repo -it dolos
```
<!-- TODO(b/438110087) Update this section when changing default flow -->

To build the bootloader-enabled version of the firmware you need to pass an additional environment variable, `BUILD_MODE` to specify what kind of build you prefer:
- `COMBINED`: builds both bootloader and firmware and generates a combined binary
- `BOOTLOADER`: builds just the bootloader
- `FIRMWARE`: builds just the firmware

Any of the below is a valid invocation:

```shell
docker run -e BUILD_MODE="COMBINED" --rm --user $(id -u):$(id -g) -v `pwd`:/repo -it dolos
docker run -e BUILD_MODE="BOOTLOADER" --rm --user $(id -u):$(id -g) -v `pwd`:/repo -it dolos
docker run -e BUILD_MODE="FIRMWARE" --rm --user $(id -u):$(id -g) -v `pwd`:/repo -it dolos
```

Additionally if you want to enable developer mode, which skips the boot timeout on the bootloader, simply add the `DEV_MODE` environment variable with the value `enabled` as such:
```shell
docker run -e BUILD_MODE="BOOTLOADER" -e DEV_MODE="enabled" --rm --user $(id -u):$(id -g) -v `pwd`:/repo -it dolos
```

Please note this option is only applied when building and flashing the bootloader.

In case of any compilation error, you may need to remove the old build directory
that was created before building the docker image. Just execute:
```shell
rm -rf firmware-zephyr/build
```
And then try executing the `docker run ...` command again to compile the
firmware.

It will create the `zephyr.txt` firmware file in the
`firmware-zephyr/build/zephyr` directory.
All other build artifacts will be present in the `firmware-zephyr/build` in case
of the need of debugging the firmware.

Build the updater.

```shell
export DOCKER_BUILDKIT=1
docker build --output "./build_result" \
             --target copytohost \
             -f dockerfiles/Dockerfile.updater .
```

The output of the build(s) should be in the build_result directory.

To flash the legacy firmware built earlier, you need to execute:
```shell
./build_result/fw-updater firmware-zephyr/build/zephyr/zephyr.txt
```

<!-- TODO(b/438110087) Update this section when changing default flow -->

To flash a combined image of bootloader and firmware execute:
```shell
./build_result/fw-updater firmware-zephyr/build/zephyr/combined.txt
```

To flash just the bootloader execute:
```shell
./build_result/fw-updater bootloader-zephyr/build/zephyr/boot.txt
```

To flash just the firmware execute:
```shell
./build_result/fw-updater firmware-zephyr/build/zephyr/firmware.txt
```
