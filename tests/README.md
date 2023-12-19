# Setup
```
pip install -U pytest
pip install --upgrade --force-reinstall pyserial
```

# Running Tests
Connect Dolos to a Chromebook and run the following command.
At this time don't connect the charger cable.
```
pytest -s
```
Verify all the tests are passed.

Now charger to Dolos and run the following command.
```
pytest -s --charger-plugged
```
