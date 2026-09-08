# NPU-Check plog 检查

本样例验证 NPU_CHECK、ACLSAN 和 CLI 的内部日志写入 CANN Host plog，
而检测报告仍通过 CLI 输出通道输出。样例依次初始化 ACL、启动一个 AIV kernel、
执行同步并完成 ACL 清理。

构建并安装当前软件包后，运行样例：

```bash
source /usr/local/Ascend/cann/set_env.sh
bash npu_tools/npu_sanitizer/demo/build.sh
bash npu_tools/npu_sanitizer/demo/examples/plog_check/run.sh
```

运行脚本设置 `ASCEND_SLOG_PRINT_TO_STDOUT=0`，默认将 `ASCEND_GLOBAL_LOG_LEVEL` 设为 `0`
（DEBUG）。如果调用者未指定 `ASCEND_PROCESS_LOG_PATH`，则将其设为样例独立的
`build/plog` 目录。设置 `ASCEND_GLOBAL_LOG_LEVEL=1` 时，仅检查 INFO 级别的生命周期日志。
应用进程的 PID 写入 `build/application.pid`，CLI 的 PID 在启动时获取。
不使用自定义内部日志文件 `npu_check.log` 或自定义日志级别开关。
脚本的终端及检测输出保存在 `build/plog_check.log`。

脚本最多等待 `PLOG_FLUSH_TIMEOUT` 秒（默认 10 秒），查找对应 PID 的日志文件：

```text
build/plog/debug/plog/plog-<pid>_*.log
build/plog/run/plog/plog-<pid>_*.log
```

校验要求日志包含初始化、同步、完成、API 订阅和取消订阅记录。
DEBUG 级别下还会检查 CLI 接收结果的日志。若出现自定义内部日志文件
`work-dir/npu_check.log`，则校验失败。内部详细信息请直接查看 plog 文件，
这些信息不会复制到检测输出中。本样例要求启用 Host plog 文件输出，
不包含在 `run_smoke.sh` 的执行清单中。
