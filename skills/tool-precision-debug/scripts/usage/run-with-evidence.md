# 证据执行器：保留命令、日志与真实退出状态

运行工程的构建、复现、实验或回归命令，同时保存 stdout/stderr 合并日志、工作目录、时间、真实退出码和文件摘要。将本次要执行的工程命令直接传入，输出用于回放结果、排查失败与关联本轮产物。

## 使用与输入

`<artifact-dir>` 使用[任务产物根目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径。`--log`、`--metadata` 若传相对路径，按脚本启动目录解析；`--cwd` 只设置被测命令的工作目录。

```text
python "<skill-root>/scripts/debug/run_with_evidence.py" \
  --log "<artifact-dir>/run.log" --metadata "<artifact-dir>/run.meta.json" \
  --cwd "<operator-project>" --timeout <seconds> --tail-lines 40 \
  -- <command> <arg> ...
```

使用新日志路径；确定旧证据可替换时才加 `--overwrite`。长命令按任务耗时设置超时，`--heartbeat-interval` 可调整进度提示，传 `0` 关闭；heartbeat 在 stderr 中输出，不写入证据日志。

## 输出怎样解释

| 结果 | 含义 |
| --- | --- |
| 正常启动后的退出码 | 保留真实子进程结果；信号退出归一为 `128 + signal` |
| `124` | 执行超时，报告保留本次子进程组清理结果 |
| `127 / 126` | 可执行文件不存在 / 其他启动失败 |
| `--validation-target` 的 `adopted: true` | 只确认当前支持的直接执行文件关系；不能证明框架动态加载了某个 NPU Kernel |

进程返回 0 只说明命令成功退出，数值是否通过要看真正的 evaluator。使用 `--tail-lines` 查看末尾；需要 shell 管道时在该脚本中设置 `pipefail`，避免尾端命令掩盖失败。

元数据同时保存 `started_at_ns`、`finished_at_ns`，便于关联本轮生成的验收报告；时间落在运行区间内仍不单独证明输出语义或产物采用正确。

命令按本次子进程的有效 PATH 与 `--cwd` 解析，包括 `--env PATH=...` 和相对 PATH 项。`executable.path` 记录解析后的文件身份，`launch_path` 保留启动路径以维持虚拟环境等符号链接语义；原始参数保留在 `command_argv`。启动失败时不报告目标已采用。

## 工具绑定与依据

上面的命令可以直接执行。将本次运行与已有能力探测关联时，组合 `--probe-report`、`--require-capability`、`--expected-cann` 与 `--npu-arch`。可重复的 `--env NAME=VALUE` 用于本次子进程环境，元数据保存变量名和值的哈希。参数与支持的直接执行形式见[当前实现](../debug/run_with_evidence.py)。

能力获取见[工具探测](asc-tools-probe.md)，动态加载与产物身份见[复现性方法](../../references/diagnosis/reproducibility.md)，回放与回归见[工作流阶段 5](../../SKILL.md#六阶段调试流程)。
