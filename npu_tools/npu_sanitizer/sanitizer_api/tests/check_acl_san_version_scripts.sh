# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(NPU_CHECK_REPORT_RENDERER_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report_renderer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/report_catalog.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/report_fields.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/report_normalizer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/report_summary.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/report_text_renderer.cpp
  ${CMAKE_CURRENT_LIST_DIR}/../src/diagnostic/report/synccheck_adapter.cpp
)
