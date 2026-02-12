#!/usr/bin/python
# -*- coding: utf-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import os
import urllib.request
import argparse


def download_files_native(url_list, target_dir):
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
        print(f"Created directory: {target_dir}")

    for url in url_list:
        try:
            file_name = url.split('/')[-1]
            if not file_name or file_name == "":
                file_name = "downloaded_file"
            file_path = os.path.join(target_dir, file_name)
            print(f"Start download {url}")

            urllib.request.urlretrieve(url, file_path)
            print(f"Successfully saved to {file_path}")

        except Exception as e:
            print(f"Download file from {url} failed: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Download files from URLs.")
    parser.add_argument('--dest_dir', '-d', type=str, default='.', 
                        help='Target directory to save files (default: current directory)')
    args = parser.parse_args()

    tar_urls = [
        "https://gitcode.com/cann-src-third-party/makeself/releases/download/release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz",
        "https://gitcode.com/cann-src-third-party/boost/releases/download/v1.87.0/boost_1_87_0.tar.gz",
        "https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz",
        "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h2/mockcpp-2.7.tar.gz",
        "https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h3/mockcpp-2.7_py3-h3.patch"
    ]

    download_files_native(tar_urls, args.dest_dir)