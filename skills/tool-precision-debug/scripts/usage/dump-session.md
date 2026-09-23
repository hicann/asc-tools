# DumpTensor 会话：关联采集、解析与恢复

将插桩前源码、本轮 dump、解析结果与恢复后的回放关联为同一次采集记录。会话工具负责步骤记录和对应关系检查；插桩、运行及解析通过[采集与解析流程](kernel-debug-data-parse.md#采集与直接解析)中的相应工具完成。

## 准备与采集

先按 [DumpTensor 方法](../../references/diagnosis/dumptensor.md)选择观察点。以下新增文件均使用[任务产物根目录](../../references/diagnosis/reporting.md#任务产物目录)下的路径：

```text
python "<skill-root>/scripts/debug/dump_session.py" prepare \
  --session "<artifact-dir>/dump-session.json" --config "<artifact-dir>/acl.json" \
  --dump-dir "<artifact-dir>/dump" --cann <version> --npu-arch <arch> --source "<kernel-source>"
```

将临时配置接入工程实际加载入口；dump 子目录须新建或为空。`--source` 可重复指定，记录插桩前文件供恢复核对；备份文件按需放在产物目录。安装不明时使用 probe 查询；显式提供 `--probe-report` 后，后续执行与解析需保持同一绑定；未提供时版本与架构只是声明。

完成插桩和构建后，运行原失败入口并关联采集：

```text
python "<skill-root>/scripts/debug/run_with_evidence.py" \
  --env "ASCEND_DUMP_PATH=<artifact-dir>/dump" --log "<artifact-dir>/npu.log" \
  --metadata "<artifact-dir>/npu.meta.json" -- <npu-command> <arg> ...
python "<skill-root>/scripts/debug/dump_session.py" collect \
  --session "<artifact-dir>/dump-session.json" --run-metadata "<artifact-dir>/npu.meta.json"
```

会话通过执行记录中的 `ASCEND_DUMP_PATH` 与新鲜目录关联 dump。若选择 probe 绑定，运行器另加相同 `--probe-report`、`--require-capability show-kernel-debug-data`、`--expected-cann` 和 `--npu-arch`。随后用[解析适配器](kernel-debug-data-parse.md)记录官方解析器与输入；有 probe 时同样绑定，无 probe 时显式指定 `--tool` 即可。比较按所选标准执行，检查点工具复用已有结果。

## 有效数值失败

`collect` 和 `close` 默认要求正常退出 0。工程明确以非零码表示数值失败时，附加 `--mismatch-exit-code <code> --evaluation-report <本次执行生成的报告>`；报告格式见[外部结果](repeatability-analyzer.md#外部结果格式)。

只有正常结束、退出码已声明、报告来自本轮且有效判定失败时才接受，原始退出码和验收报告副本会保留。超时、信号终止、陈旧报告及相互矛盾的结果不放行。优先使用当前执行器的纳秒时间记录；旧记录时间精度不足以绑定报告时需重新取得可信执行记录。

## 恢复与收口

恢复插桩前源码，完成干净构建和原失败回放：

```text
python "<skill-root>/scripts/debug/dump_session.py" close \
  --session "<artifact-dir>/dump-session.json" --parser-report "<artifact-dir>/parse-report.json" \
  --checkpoint-report "<artifact-dir>/checkpoint-analysis.json" \
  --clean-build-metadata "<artifact-dir>/clean-build.meta.json" --clean-replay-metadata "<artifact-dir>/clean-replay.meta.json"
```

回放仍按精度失败退出时，按上一节附加退出码及此次回放的验收报告。干净构建仍须成功。会话核对采集/解析对应关系、源码恢复和构建回放顺序；关闭表示取证过程已收口，不要求 checkpoint 全部通过，也不表示算子已经修复。实际工具入口与声明/探测范围分别保留。

全部参数与状态见[实现](../debug/dump_session.py)。
