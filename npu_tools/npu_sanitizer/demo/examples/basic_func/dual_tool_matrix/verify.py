# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import re
import sys
from pathlib import Path

expected = {
    "valid": (0, 0, 1),
    "mem_only": (1, 0, 1),
    "sync_only": (0, 1, 1),
    "both": (1, 1, 1),
    "same_stream": (2, 2, 1),
    "multi_stream": (1, 1, 2),
}
case, status, path = sys.argv[1:]
text = Path(path).read_text()
mem, sync, synchronizations = expected[case]
assert int(status) == 0, text
assert f"COMPOSITE_RESULT case={case} " in text and "result=pass" in text, text
for tool, errors in (("memcheck", mem), ("synccheck", sync)):
    rows = re.findall(rf"^tool={tool} (.+)$", text, re.M)
    assert len(rows) == 1, text
    fields = dict(re.findall(r"(\w+)=(\d+)", rows[0]))
    assert int(fields["errors"]) == errors, text
    assert int(fields["warnings"]) == 0, text
    assert int(fields["synchronizations"]) == synchronizations, text
    for field in (
        "pending_device_operations",
        "dropped_device_operations",
        "pending_opens",
    ):
        assert int(fields.get(field, "0")) == 0, text
assert text.count("ERROR:[MEMCHECK] Invalid GM read of size 32 bytes") == mem, text
assert (
    text.count(
        "ERROR:[SYNCCHECK] Synchronization pairing mismatch: redundant SET_FLAG."
    )
    == sync
), text
assert len(re.findall(r"^========= ERROR:", text, re.M)) == mem + sync, text
assert "malformed_callbacks=0 framework_errors=0 dropped_messages=0" in text, text
assert (
    "status=complete aclsan_unsubscribe=0 dropped_messages=0 analysis_complete=true report_truncated=false"
    in text
), text
assert (
    f"[CLI] outcome=forwarded has_errors={int(bool(mem or sync))} truncated=0 child_exit=0 exit=0"
    in text
), text
print(
    f"PASS {case}: memcheck={mem} synccheck={sync} synchronizations={synchronizations}"
)
