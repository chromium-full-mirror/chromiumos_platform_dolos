# Copyright The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from doloscmd.download import BUCKET_URL
import pytest
import requests


FILE_LIST_RESPONSE = """<?xml version='1.0' encoding='UTF-8'?>
<ListBucketResult xmlns='http://doc.s3.amazonaws.com/2006-03-01'>
  <Name>dolos-firmware</Name>
  <Prefix></Prefix>
  <Marker></Marker>
  <IsTruncated>false</IsTruncated>
  <Contents>
    <Key>0.170.0-3b80f28/zephyr.txt</Key>
  </Contents>
  <Contents>
    <Key>cable_eeprom/model_a/model_a_v1.yaml</Key>
    <Generation>1724433686019527</Generation>
    <MetaGeneration>1</MetaGeneration>
    <LastModified>2024-08-23T17:21:26.127Z</LastModified>
    <ETag>"2115344fc8e56db111ab37a17e1776cc"</ETag>
    <Size>1175</Size>
  </Contents>
  <Contents>
    <Key>cable_eeprom/model_b/model_b_v1.yaml</Key>
    <Generation>1724433648145180</Generation>
    <MetaGeneration>1</MetaGeneration>
    <LastModified>2024-08-23T17:20:48.260Z</LastModified>
    <ETag>"ca343cdcc5eaa2f6bb063e94a906ae34"</ETag>
    <Size>1185</Size>
  </Contents>
  <Contents>
    <Key>cable_eeprom/model_b/model_b_v2.yaml</Key>
    <Generation>1724433648145180</Generation>
    <MetaGeneration>1</MetaGeneration>
    <LastModified>2024-08-23T17:20:48.260Z</LastModified>
    <ETag>"ca343cdcc5eaa2f6bb063e94a906ae34"</ETag>
    <Size>1185</Size>
  </Contents>
</ListBucketResult>
"""

YAML_RESPONSE = """
Comment: model
Version: 1
"""


def mock_requests_get(url):
    if url == BUCKET_URL:
        return FILE_LIST_RESPONSE.encode()
    else:
        # The yaml response will report what file was fetched
        # by storing it in a field for the test.
        name = url.split("/")[-1]
        response = YAML_RESPONSE + f"\nFileName: {name}"
        return response.encode()


@pytest.fixture()
def mock_storage_response(class_mocker):
    class_mocker.patch("doloscmd.download.get_response", side_effect=mock_requests_get)
