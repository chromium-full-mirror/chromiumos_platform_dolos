# Using TI Zephyr repo
- Go through the Getting Started Guide to get Zephyr an build a sample project to verify that everything is working fine:

https://docs.zephyrproject.org/latest/develop/getting_started/index.html

- We need to clone the pull request from TI to include support for MSPM0 devices:

Go to Zephyr directory and pull the pull request:
```shell
git fetch origin pull/68503/head:mspm0_support
git checkout mspm0_support
```


- Under Zephyr directory, open the west.yml file and update the hal_ti section. You need to add url to point to the hal_ti MSPM0 support pull request:

It should look like this:

```yaml
    - name: hal_ti
      revision: pull/51/head
      path: modules/hal/ti
      groups:
        - hal
```


- Run the command:

 ```shell
 west update
 ```

This will update the modules (e.g. clones hal_ti and other modules)

- Set the environment variable ``ZEPHYR_BASE`` to point to the zephyr installation:

```shell
export ZEPHYR_BASE=~/zephyrproject/zephyr/
```

# Building dolos zephyr PoC

- Navigate to Dolos Zephyr firmware directory.
- Build the project using the following command (if you are using a virtual environment make sure to enable it first):

```shell
west build -p
```

- Set the environment variable ``CCS_BASE`` to point to CCS directory. A Kconfig variable ``CONFIG_MSPM0_IMAGE_OUTPUT_TI_TXT`` will use it to generate the TI_TXT file:

```shell
export CCS_BASE="~/ti/ccs1250"
```

- There's a known issue where the TI TXT file produced by the tool may not be 8-byte aligned, to fix the file:
    - Open the ./build/zephyr/zephyr.txt file.
    - For each hex line before a new ``@address`` or ``q`` statement, make sure that the line contains either 8 or 16 hex characters.
    - If the hex characters are insufficient, add ``00`` until the file is 8 or 16 hex charaters long.

- Flash the zephyr.txt
