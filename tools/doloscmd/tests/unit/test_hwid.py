# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from doloscmd.error import DolosConsoleError
from doloscmd.hwid import HWIDV3
import pytest


class TestHWID:
    def test_valid_model(self):
        """Verify we can extract the model name correctly."""
        test_serials = {
            "MODELNAME A2AA2AA2AA2AA2AI9I": "modelname",
            "MODELNAME A2A-A2A-A2A-A2A-A2A-I9I": "modelname",
            "MODELNAME-ABC DEF-GHI A2AA2AA2AA2AA2AI3V": "modelname",
            "MODELNAME-ABC DEF-GHI A2AA2AA2AA2AA2AI3V": "modelname",
        }
        for serial, exp_val in test_serials.items():
            hwid = HWIDV3(serial)
            assert hwid.model == exp_val

    def test_invalid_serials(self):
        """Verify we can detect invalid formats and checksums."""
        test_serials = [
            "MODELA2AA2AA2AA2AA2AI9I",  # Missing space
            "A2A-A2A-A2A-A2A-A2A-I3V",  # Missing model field
            "MODEL A2A-A2A-A2A-A2A-A2A-I3",  # Non-multiple of 3
            "MODEL A2A-A2A-A2A-A2A-A2A-I3VA",  # Non-multiple of 3
            "MODEL A2AA2AAAAA2A",  # Invalid character
            "MODEL A2AA2AA29A2A",  # Invalid character
            "MODEL A2AA2AA2AA2A",  # Bad checksum
        ]
        for serial in test_serials:
            with pytest.raises(DolosConsoleError):
                hwid = HWIDV3(serial)

    def test_revision_id(self):
        """Verify we can extract the revision id correctly."""
        test_serials = {
            "MODEL A2AA9AA2AA2AA2AI7Q": 0,
            "MODEL B2AA4AA2AA2AA2AI7M": 1,
            "MODEL C2AA5AA2AA2AA2AI4S": 2,
            "MODEL D2AA2AA2BA2AA2AI4L": 3,
            "MODEL P2AA2AA2CA2AA2AI8M": 15,
            "MODEL Z2AA2AA2AA2AA2AI65": 25,
        }
        for serial, exp_val in test_serials.items():
            hwid = HWIDV3(serial)
            assert hwid.revision_id == exp_val

    def test_bit_parsing(self):
        """Verify we can decode the bitpattern correctly."""
        test_serials = {
            "MODEL C7ZE44L3566FN3UT8H": "000101011100100100010111000101100111101111101000010101101001101001001",
            "MODEL D4P69ET7NS6BZ3R756": "000110100111111110111001001001110101101100101000000111001001100011111",
            "MODEL A8TJ3H526P5NG6FC4B": "00000110100110100100100111111010001111001111011011010011010000101000",
            "MODEL B3QW9X552D4KZ8378L": "000010011000010110111101111110101111010000110100101011001110110111111",
            "MODEL C8BD4XL4IO8F76KR9T": "000101100000100011010101110101101001000011101100010111111100010101000",
        }
        for serial, exp_val in test_serials.items():
            hwid = HWIDV3(serial)
            assert hwid.decoded_bits == exp_val

    def test_single_pattern_battery_extraction(self):
        """Verify we can parse a bitfield."""
        patterns = [{"ids": (0, 1, 2, 3, 4), "bits": (65, 61, 54, 55)}]
        test_serials = {
            # Simple test cases where we can ignore the last bit trimming
            "MODEL A2AA2AA2AA2AA2AI76": 0,
            "MODEL A2AA2AA2AA2AC2AI3V": 1,
            "MODEL A2AA2AA2AA2AE2AI9J": 2,
            "MODEL A2AA2AA2AA2AW2AI95": 3,
            "MODEL A2AA2AA2AA2AA2II7W": 4,
            "MODEL A2AA2AA2AA2AA2AY82": 8,
            "MODEL A2AA2AA2AA2AE2AY6N": 10,
            # Requires final bit trimming to be parsed correctly
            "MODEL B2BB2BC2CB2KB3IA8H": 0,
            "MODEL D4BF4EF3FB2KL3IQ2Q": 5,
        }
        for serial, exp_val in test_serials.items():
            hwid = HWIDV3(serial)
            assert hwid.read_bitfield(patterns) == exp_val, serial

    def test_multi_pattern_battery_extraction(self):
        """Verify the id detection works with patterns."""
        patterns = [
            {"ids": (0,), "bits": ()},
            {"ids": (1, 2), "bits": (19, 20)},
            {"ids": (3,), "bits": (38,)},
            {"ids": (15,), "bits": (37,)},
        ]
        test_serials = {
            "MODEL A2AA9AA2AA2AA2AI7Q": None,
            "MODEL B2AA4AA2AA2AA2AI7M": 2,
            "MODEL C2AA5AA2AA2AA2AI4S": 3,
            "MODEL D2AA2AA2BA2AA2AI4L": 1,
            "MODEL P2AA2AA2CA2AA2AI8M": 1,
            "MODEL Z2AA2AA2AA2AA2AI65": DolosConsoleError,
        }

        for serial, exp_val in test_serials.items():
            hwid = HWIDV3(serial)
            if exp_val is not DolosConsoleError:
                assert hwid.read_bitfield(patterns) == exp_val, serial
            else:
                with pytest.raises(DolosConsoleError):
                    hwid.read_bitfield(patterns)
