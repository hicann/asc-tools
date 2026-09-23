# 重复运行分析：验收结果、漂移与偶发

对固定输入执行多轮比较，汇总失败率、错误位置和幅度变化，帮助区分稳定失配、偶发与漂移。可以直接接入工程或官方比较器的每轮结果，沿用其验收规则；随机算子的分布检验另按[比较与验收](../../references/diagnosis/baseline.md#随机输出边际分布与序列义务分开验证)选择方法。

## 复用外部验收结果

让被测命令每轮写出真实验收 JSON，再指定其位置：

```text
python "<skill-root>/scripts/analysis/repeatability_analyzer.py" \
  --runs <N> --work-dir "<artifact-dir>/repeatability-run" --output "<artifact-dir>/repeatability.json" \
  --input <fixed-input> --evaluation-report "<artifact-dir>/evaluation.json" \
  --mismatch-exit-code 1 -- <工程运行并验收的命令> <arg> ...
```

仅当工程明确用退出码 1 表示数值失败时才加示例中的选项；可重复声明其他数值失败退出码，默认仅接受正常退出 0。非零数值失败还必须正常结束，并产生新鲜、有效且判定失败的报告。超时、信号终止、陈旧结果、未声明退出码或退出码与判定矛盾均记为无效观察，保留真实退出状态。

此模式不调用自有比较器。`--actual` 可选，提供时逐轮检查刷新并归档；`--golden` 可选，提供时检查参考文件未变。dtype、shape、mode、容差及有效区的数值处理由原比较器负责，本工具不会用这些选项重判外部结果；`--mask` 若提供，仅作为固定输入检查。

### 外部结果格式

可以直接保存官方检查函数返回的字典；最低要求是布尔判定，例如 `{"is_pass": false}`。也兼容现有比较报告的 `status: PASS/MISMATCH`（以及 `FAIL`）。同时提供两个判定字段时必须一致；字符串 `"false"` 不作为布尔值接受。

原 JSON 文件完整归档，额外字段保留。分析器可读取顶层或 `metrics` 中已有的 `max_abs_error`、`matched_ratio` 等指标及完整失配位置集合的 `mismatch_index_sha256`；缺项或非有限误差指标保留未知，不从通过比例或展示用 top-k 推断全部错误位置。项目格式不同时，用任务目录中的少量转换代码输出 `is_pass` 并引用原报告即可。

## 自有逐元素诊断模式

没有外部验收报告、且需要本工具支持的比较方式时，旧调用继续可用：

```text
python "<skill-root>/scripts/analysis/repeatability_analyzer.py" \
  --runs <N> --work-dir "<artifact-dir>/repeatability-run" --output "<artifact-dir>/repeatability.json" \
  --input <fixed-input> --actual "<artifact-dir>/actual.npy" --golden <golden.npy> \
  --rtol <已确认的值> --atol <已确认的值> -- <command> <arg> ...
```

该模式调用[自有比较器](precision-compare.md)，其全部选定元素通过规则不自动等价于比例、误差上限或其他项目标准。

## 输出怎样解释

| 分类 | 本批观察与下一步 |
| --- | --- |
| `STABLE_MISMATCH` | 完整失配位置集合与已记录误差摘要相同，可固定坐标继续观察 |
| `POSITION_DRIFT` | 完整失配位置集合变化，核对地址、所有权与状态 |
| `AMPLITUDE_DRIFT` | 位置集合相同而误差摘要变化，比较数值顺序、状态和环境 |
| `ALL_RUNS_FAILED` | 所有有效轮次均验收失败，但缺少完整位置/误差观察，不能判断失配稳定或漂移 |
| `INTERMITTENT_FAILURE` | 同一批次既有通过又有失败，保留失败并寻找触发条件 |
| `NO_FAILURE_OBSERVED` | 本批均按所选判据通过，不证明输出逐位稳定，也不覆盖历史失败 |
| `INCONCLUSIVE` | 至少一轮执行、刷新或比较观察无效，先补对应信息 |

分类完成返回 0，包含无效轮次返回 1，输入配置无效返回 2；分析器退出码不代替被测命令或精度结果。

`<artifact-dir>` 遵循[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)。`--work-dir` 使用尚不存在的子目录，汇总输出位于其外侧、仍在产物根目录内。固定输入、参考与命令；输出不能覆盖固定输入。每轮保存日志、验收原文及提供的输出，输入或参考被改写时停止解释。产物是否实际采用仍结合工程加载记录核对。

分类只描述本批样本。后续采样与定位见[复现性方法](../../references/diagnosis/reproducibility.md)及[假设与实验](../../references/diagnosis/hypothesis-experiment.md)；全部参数见[实现](../analysis/repeatability_analyzer.py)。
