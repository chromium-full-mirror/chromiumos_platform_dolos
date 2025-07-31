# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Configure logging format and verbosity."""

import logging


LOG_FORMAT = "%(levelname)s - %(filename)s:%(lineno)d:%(funcName)s : %(message)s"


def set_config(verbose=False):
    """Configure logging levels

    Args:
        verbose (bool): Enable or disable verbose logs
    """
    if verbose:
        logging.basicConfig(level=logging.DEBUG, format=LOG_FORMAT)
    else:
        logging.basicConfig(level=logging.INFO, format=LOG_FORMAT)
