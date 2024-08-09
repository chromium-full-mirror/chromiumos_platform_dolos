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

Build the firmware.
```shell
export DOCKER_BUILDKIT=1
docker build --output "./build_result" \
             --target copytohost \
             --builder=buildkit \
             -f dockerfiles/Dockerfile.firmware .
```

Build the updater.

```shell
docker build --output "./build_result" \
             --target copytohost \
             --builder=buildkit \
             -f dockerfiles/Dockerfile.updater .
```

The output of the build(s) should be in the build_result directory.
