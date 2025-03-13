# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a mechanism to parse the HWID field and extract the battery."""

from zlib import crc32

from doloscmd.error import DolosConsoleError


class HWIDV3:
    """Handle parsing HWIDv3 formats."""

    BASE8_ALPHABET = "23456789"
    BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

    def __init__(self, hwid):
        """Creates and validates a new HWID from the string.

        Args:
            hwid (str): Input HWID

        Raises:
            DolosConsoleError: Validation has failed
        """
        hwid_parts = hwid.upper().split()
        # Verify all parts of the ID field are included
        if len(hwid_parts) == 1:
            raise DolosConsoleError("HWID {hwid!r} is missing components")

        # Everything before the final space is part of the full model string
        self.full_model_str = " ".join(hwid_parts[:-1])
        # The encoded string is the final data payload section which contains
        # characters grouped into chunks of 3
        self.encoded_str = hwid_parts[-1].replace("-", "")
        # Decoded bits store HWID data fields as individual bits
        self.decoded_bits = self._parse_id()
        self._validate_checksum()

    @property
    def full_hwid_str(self):
        """Return the full hwid string."""
        return f"{self.full_model_str} {self.encoded_str}"

    @property
    def model(self):
        """Returns the DUT model."""
        return self.full_model_str.replace("-", " ").split(maxsplit=1)[0].lower()

    @property
    def revision_id(self):
        """Returns the revision id."""
        return int(self.decoded_bits[:5], 2)

    @property
    def data_str(self):
        """Returns the data segment of the hwid."""
        return self.encoded_str[:-2]

    @property
    def crc_str(self):
        """Returns the crc segment of the hwid."""
        return self.encoded_str[-2:]

    def _parse_id(self):
        """Parse the id string into bits."""

        # Verify the data segment is complete
        if len(self.encoded_str) % 3 != 0:
            raise DolosConsoleError(
                f"Hardware ID {self.encoded_str!r} is not multiples of 3"
            )

        decoded_bits = ""

        # Characters are encoded in groups of 3 representing a 5:3:5 bits each.
        # The numeric value is extracted from the index of the Base32 and Base8
        # alphabets and converted into binary. We skip the last 2 characters
        # used the in the CRC.
        for i, char in enumerate(self.data_str):
            delta_bits = ""
            try:
                if i % 3 != 1:
                    delta_bits = f"{self.BASE32_ALPHABET.index(char):05b}"
                else:
                    delta_bits = f"{self.BASE8_ALPHABET.index(char):03b}"
            except ValueError as err:
                raise DolosConsoleError(
                    f"Invalid HWID character: {char!r} at index {i}"
                ) from err
            decoded_bits += delta_bits
        # The very last '1' marks the end of the string and is removed
        decoded_bits = decoded_bits[: decoded_bits.rfind("1")]
        return decoded_bits

    def __getitem__(self, idx):
        """Returns the bit at a given index."""
        if idx >= len(self.decoded_bits):
            return "0"
        return self.decoded_bits[idx]

    def _calculate_checksum(self, data_str=None):
        """Calculate the expected checksum"""
        if data_str is None:
            data_str = self.data_str
        crc_input = f"{self.full_model_str} {data_str}"
        print("CRC input", crc_input)
        crc8 = crc32(crc_input.encode("utf-8")) & 0xFF
        crc = self.BASE8_ALPHABET[crc8 >> 5]
        crc += self.BASE32_ALPHABET[crc8 & ((1 << 5) - 1)]
        return crc

    def _validate_checksum(self):
        """Verify the ID is valid"""
        exp_crc = self._calculate_checksum()
        act_crc = self.encoded_str[-2:]
        if exp_crc != act_crc:
            raise DolosConsoleError(
                f"CRC Error: HWID:{self.full_hwid_str} Expected {exp_crc!r} and Actual {act_crc!r}"
            )

    def _read_bitfield(self, idxs):
        """Reads the bitfield

        Returns:
            Numeric field value
        """
        if len(idxs) == 0:
            return 0
        bits = ""
        for x in idxs:
            bits += self[x]
        return int(bits, 2)

    def read_bitfield(self, patterns):
        """Parse the bitfield to obtain the numeric value

        Accepts a list of entries and identifies the decoding pattern
        Example input: [{'ids': (0, 1, 2, 3, 4), 'bits': (65, 61, 54, 55)}]

        Returns:
            Numeric field value or None if the bitfield is empty
        Raises:
            Exception: HWID revision field is unknown
        """
        for pattern in patterns:
            if self.revision_id in pattern["ids"]:
                bits = pattern["bits"]
                if bits:
                    return self._read_bitfield(pattern["bits"])
                return None

        raise DolosConsoleError(f"Unknown revision id {self.revision_id}")
