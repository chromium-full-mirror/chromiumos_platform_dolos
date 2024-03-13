# Using TI Zephyr repo
- Go through the **Getting Started Guide**, it has steps for installing Zephyr SDK and Zephyr required libraries that will be needed. You should follow all the installtion steps except for **Get Zephyr and install Python dependencies: step 5**, we will use the steps below instead:

https://docs.zephyrproject.org/latest/develop/getting_started/index.html

- Instead of pulling the Zephyr repo, we need to clone the TI Zephyr repo, which contains the support for the MSPM0 devices:

```shell
west init -m  https://github.com/msp-ti/zephyr.git --mr mspm0_cleanup ~/zephyrproject
```

- Under ``~/zephyrproject/zephyr`` directory, open the ``west.yml`` file and update the ``hal_ti`` section. You need to add ``url`` property to point to the **hal_ti** project (https://github.com/msp-ti/hal_ti) and change the revision to point to the branch ``mspm0_support``:

It should look like this:

```yaml
    - name: hal_ti
      revision: mspm0_support
      url: https://github.com/msp-ti/hal_ti
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

- Go back to the **Getting Started Guide** and continue with the steps

# Building Dolos

- Navigate to Dolos Zephyr firmware directory.
- To get the ``zephyr.txt`` file, you need to set the environment variable ``CCS_BASE`` to point to CCS directory. A Kconfig variable ``CONFIG_MSPM0_IMAGE_OUTPUT_TI_TXT`` will use it to generate the TI_TXT file once you build the project:

```shell
export CCS_BASE=~/ti/ccs1250
```

- Build the project using the provided build script under ``src/`` (if you are using a virtual environment make sure to enable it first):

```shell
./build.sh
```

- There's a known issue where the TI TXT file produced by the tool may not be 8-byte aligned, to fix the file:
    - Open the ./build/zephyr/zephyr.txt file.
    - For each hex line before a new ``@address`` or ``q`` statement, make sure that the line contains either 8 or 16 hex characters.
    - If the hex characters are insufficient, add ``00`` until the file is 8 or 16 hex charaters long.

- Flash the ``zephyr.txt``
