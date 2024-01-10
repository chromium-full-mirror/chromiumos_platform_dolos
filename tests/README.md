# Running the tests using Python Wheel
## Building the whl file

Run the **build_whl.sh** script to build the Python whl file. The whl file will be created under the **dist** directory.

## Installing the whl
To install the whl, run the following command:

***pip install dolos_factory_tests-1.0-py3-none-any.whl***

## Running the tests

-Connect Dolos to Chromebook.

-Connect host USB and charger to Dolos.

-At the command prompt run the command **run_dolos_tests**. The default UART baudrate is 115200. If the Dolos has different baudrate, provide it as below:

***run_dolos_tests --baudrate 9600***

-Once Dolos is detected, the tests will start running. The tests will take around 22 seconds to complete.

--------------------------------------------------------------------------------
# Running the tests from test source files

## Setup
```
pip install -U pytest
pip install --upgrade --force-reinstall pyserial
```

## Running Tests
-Connect Dolos to a Chromebook, connect the charger and eeprom to run all tests, or you can disconnect the eeprom to exclude the eeprom test.

-Move to tests folder then run the following command:

*python run_tests.py*

-If your Dolos baudrate value is not 115200 - which is the default in the script - then you need to change
the baudrate value in the command line like this: (for example baudrate is 9600)

  *python run_tests.py --baudrate 9600*


Verify all the tests are passed.

-To run all the tests in test_factory.py without the use of run_tests script, run the command:

*pytest -s*

-To run a specific test, you can specify the test name(for example test_pac)

   *pytest -sk test_pac*
