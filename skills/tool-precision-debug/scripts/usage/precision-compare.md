# 精度比较器：有效区、比较模式与结果解释

比较 actual 与 golden，获取首个失配位置、错值样本、误差指标和位模式差异。可直接分析工程输出或中间 Tensor；按本次要观察的数值或位差选择模式，正式验收与诊断指标的对应关系见下方[结果解释](#模式与输出怎样解释)。

## 使用与输入

`<artifact-dir>` 使用[任务产物根目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径。

```text
python "<skill-root>/scripts/analysis/precision_compare.py" \
  --actual <actual.npy> --golden <golden.npy> \
  --rtol <项目要求> --atol <项目要求> \
  --output "<artifact-dir>/precision.json"
```

`.bin` 另需显式 dtype 和 shape。有效元素是 C-order 连续前缀时可用 `--valid-count`；与 padding 交错时用同 shape 的布尔 `.npy` `--mask`。选择范围应由输出语义预先确定，不能事后筛掉失败位置。

BF16 的 `.npy` 使用 uint16 存储载体，并显式声明对应侧的 `--actual-dtype bf16` 或 `--golden-dtype bf16`；`.bin` 同样声明类型与 shape。报告的 `dtype` 保留规范名称 `bfloat16`，`storage_dtype` 记录 uint16；数值分析使用解码值，位比较使用原始 16 位编码。BF16 与 FP32 可按适用的数值判据比较，但不接受跨类型的 bitwise 比较。

## 模式与输出怎样解释

| 模式或结果 | 含义与边界 |
| --- | --- |
| `auto` | 两侧均为整数/布尔时精确比较；包含浮点时使用 `abs(actual-golden) <= atol + rtol*abs(golden)` |
| `--mode exact` | 按数值精确相等比较；整数与浮点混合时保留整数低位，不因转换舍入把不同值判成相等 |
| 浮点特殊值 | NaN 与同位置 NaN 匹配，Inf 还要求符号一致；golden 为零的情况单独统计 |
| `--mode bitwise` | 要求相同 dtype，比较原始位；容差不参与判定，符号零和不同 NaN 编码也会产生位差 |
| 退出码 `0 / 1 / 2` | 指定规则下通过 / 存在失配 / 输入或比较条件无效；不是所有项目共用的验收判据 |

本工具的容差模式要求所有选定元素通过，不自动等价于“通过元素比例＋最大误差上限”、MERE/MARE、统计分布或多输出聚合标准。正式验收优先复用项目或 `/ops-precision-standard` 的适用比较方法，本工具补充错误位置与局部指标。bitwise 也可能比“NaN 均视为相等”的项目要求更严格；原始位失配总数看 `bitwise_mismatch_count`，NaN 符号/payload 等分类可能重叠。

误差摘要按 FP64 计算，可能丢失大整数低位；不以摘要零值覆盖精确失配。未定义的指标为 `null`；因范围溢出而不可表示的指标也为 `null`，并列入 `nonfinite_metric_fields`。这些辅助指标的缺失不覆盖本次 PASS/MISMATCH，继续使用失配计数、位置与原值定位。

## 继续定位与依据

- [工具实现与全部参数](../analysis/precision_compare.py)：以当前实现及 `--help` 为准，不按 dtype 自设通用阈值。
- [基线与 evaluator 校准](../../references/diagnosis/baseline.md)：确认比较语义、有效区和 oracle。
- [误差位置、尺度与数值候选](../../references/diagnosis/localization.md)：比较器只比较给定文件，不生成或确认根因假设。
- [数值相等与逐位一致](../../references/diagnosis/baseline.md#数值相等与逐位一致)：确认符号零、NaN 等值的比较规则。
