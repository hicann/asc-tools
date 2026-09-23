# Checkpoint 分析：已有结果、边界与趋势

汇总多个中间观察点的比较报告，展示各点状态和误差趋势，帮助选择下一观察位置。输入可以来自工程比较器、官方标准函数或本 Skill 的比较器；报告沿用这些已有结果，采集与数值比较由相应工具完成。

## 使用与最小计划

```text
python "<skill-root>/scripts/analysis/checkpoint_analyzer.py" \
  --plan "<artifact-dir>/checkpoint-plan.json" --output "<artifact-dir>/checkpoint-analysis.json"
```

计划可从以下最小形式开始；名称唯一，顺序默认采用列表顺序。每个结果文件使用[已有比较报告或外部验收结果](repeatability-analyzer.md#外部结果格式)。

```json
{
  "checkpoints": [
    {"checkpoint_id": "input", "semantic": "输入搬运后", "comparison_report": "input-result.json"},
    {"checkpoint_id": "reduction", "semantic": "归约后", "comparison_report": "reduce-result.json"}
  ]
}
```

相对结果路径按计划文件所在目录解析。线程编号、比较编号、源码行、张量描述和容差无需重复填写；旧计划继续支持。显式提供的 dtype、shape、选择范围或容差与报告均有相应信息时，工具核对一致性；bitwise 报告可直接接入。

未测点省略 `comparison_report`；无法有效比较时填写 `ambiguous_reason`，不伪造 PASS/FAIL。单次有效比较失败即为 FAIL，偶发或稳定性未确认不改变该判定；缺少比较或采集背景时仍展示已有状态和指标，不据此启用自动二分。

## 何时给出二分建议

需要自动建议时，为相关点补充已有的参考关系和观察背景。例如：

```json
{
  "checkpoint_id": "reduction",
  "semantic": "归约后",
  "comparison_report": "reduce-result.json",
  "oracle": {"contract_id": "同一逐阶段参考方法"},
  "comparison_basis": "同一局部比较规则与有效范围；详见任务报告",
  "collection": {"plane": "NPU", "perturbed": false}
}
```

`comparison_basis` 是实际采用的比较依据说明，不是为了启用工具随意填写的标签，也不能覆盖报告中已知的参数差异。自有比较报告或包含完整混合容差、通过比例与最大误差上限参数的报告可提供比较规则，无需再抄参数；参考关系、观察面与扰动信息仍需来自实际观察。不同依据的点先分开分析，不能仅因都叫 PASS/FAIL 就拼成边界。

| 建议状态 | 含义与下一步 |
| --- | --- |
| `ELIGIBLE` | 工具未发现所给比较/采集背景冲突，PASS→FAIL 区间内有计划未测点；核对真实依赖、稳定性与局部单调假设后可采集建议点 |
| `ADJACENT_BOUNDARY` | 当前计划的边界间没有可继续二分的未测点，不代表源码中只剩一步；按区间内容选择代码检查或补点 |
| `INELIGIBLE` | 已测点出现 FAIL 后再 PASS，或比较/采集背景不足、冲突等；先补具体缺口或换观察 |
| `INSUFFICIENT` | 缺少正常或异常边界，先补观察 |

建议仅来自计划中的未测点，按候选点数量选中点，不计算实际调查成本。未测点尚无扰动观察，可以省略 `collection.perturbed`，不需要为获得建议预填 false；采集后再判断结果是否可作边界。区间内已有模糊或受扰动观察时会阻止建议，已测点的背景缺失也需先补齐；区间外的模糊点不必阻断当前边界。工具不读取真实计算图，不验证参考独立性或未测区间的单调性，也不汇总跨轮稳定性；使用建议前按[语义二分前提](../../references/diagnosis/semantic-bisection.md#前置条件)核对，不能把建议状态当成这些前提的证明。

## 误差趋势

`first_measurable_error_checkpoint` 指首个报告了非零、有限最大绝对误差的点；`first_failed_checkpoint` 指首个按所选验收规则失败的点。两者可能不同；位差也可能没有数值绝对误差。缺指标保留未知，显示零值也不能代替原整数或位模式判定。

相邻误差差值、倍率和最大已观察增幅只整理已有指标；尺度变化、参考精度和扰动由[逐层分析方法](../../references/diagnosis/progressive-error-analysis.md)解释。首次可见误差或首次失败都不自动等于根因。

计划、报告和新增观察文件统一放在[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)，保留原始结果。观察语义见[检查点设计](../../references/diagnosis/checkpoint-design.md)，二分条件见[语义二分](../../references/diagnosis/semantic-bisection.md)，全部参数见[实现](../analysis/checkpoint_analyzer.py)。
