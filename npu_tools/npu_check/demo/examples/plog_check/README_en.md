# NPU-Check plog Check

This sample verifies that internal NPU_CHECK, ACLSAN and CLI records use CANN Host plog,
while the check report remains on the CLI output channel. It initializes ACL, launches
one AIV kernel, synchronizes, and finalizes ACL.

Build and install the current package, then run:

```bash
source /usr/local/Ascend/cann/set_env.sh
bash npu_tools/npu_check/demo/build.sh
bash npu_tools/npu_check/demo/examples/plog_check/run.sh
```

The runner uses `ASCEND_SLOG_PRINT_TO_STDOUT=0`, defaults `ASCEND_GLOBAL_LOG_LEVEL` to `0`
(DEBUG), and sets a private `ASCEND_PROCESS_LOG_PATH` at `build/plog` unless supplied
by the caller. `ASCEND_GLOBAL_LOG_LEVEL=1` checks INFO lifecycle records only.
The application's PID is written to `build/application.pid`; the CLI PID is captured
when launching it. There is no custom internal `npu_check.log` file or custom log-level
switch. The script's console/check output is retained in `build/plog_check.log`.

The runner waits up to `PLOG_FLUSH_TIMEOUT` seconds (default 10) for PID-specific logs in:

```text
build/plog/debug/plog/plog-<pid>_*.log
build/plog/run/plog/plog-<pid>_*.log
```

It requires initialization, synchronization, completion, API subscription and
unsubscription records. At DEBUG it also checks CLI result reception. It rejects a
custom internal `work-dir/npu_check.log`. Read the plog files for internal details;
they are not copied into the check output. This sample requires Host plog file output
and is not included in `run_smoke.sh`.
