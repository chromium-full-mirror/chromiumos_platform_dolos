To regenerate the compiled files (this should be updated reasonably frequently):

1. Run the command

```shell
docker run -it -v /tmp/pip:/tmp/pip ubuntu:jammy bash
```

2. Then when in that shell run:

```shell
apt-get update     && apt-get install -y     --no-install-recommends     ccache     cmake     device-tree-compiler     dfu-util     file     g++-multilib     gcc     gcc-multilib     git     gperf     libmagic1     libsdl2-dev     make     ninja-build     python3-dev     python3-pip     python3-setuptools     python3-tk     python3-wheel
pip install west
west init -m  https://github.com/msp-ti/zephyr.git --mr mspm0_dev_stable /zephyrproject
cd /zephyrproject/
west update
cd zephyr/scripts/
pip install pip-tools
mv requirements.txt requirements.in
pip-compile --generate-hashes
cp requirements.txt /tmp/pip/requirements.compiled
```

3. Finally exit the container or in another shell run below command and send changes for review:

```shell
cp /tmp/pip/requirements.compiled <dolos_src>/dockerfiles/requirements.compiled
```