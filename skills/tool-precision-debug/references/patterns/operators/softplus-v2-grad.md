# SoftplusV2Grad / SoftplusBackward 算法合同

当 SoftplusV2Grad 的输出像 `gradOutput`、像 sigmoid 中间量，或只在阈值/极负输入附近失配时，先冻结本页分段公式和每个实参的角色，再检查实现的实际数据流。本页是算法语义与定位知识；它不声明任何产品上的 ACLNN API 可用，也不迁移 RegBase 历史案例的实现或修复。

## 公式与角色

令 `bx = beta * self`。官方 SoftplusBackward 文档给出的反向公式是：

```text
gradInput = gradOutput * (1 / (1 + exp(-bx)))  if bx <= threshold
            gradOutput                         if bx > threshold
```

`gradOutput` 是上游梯度输入，`self` 是 Softplus 正向输入，`beta` 与 `threshold` 是定义公式的标量，`gradInput` 是输出。逐调用点把它们同实际形参、实参、dtype、地址/槽位及最后写者对应起来；同类型指针或寄存器位置相邻不代表角色可互换。

分支判据读取的是原始 `beta * self`：`bx > threshold` 直通 `gradOutput`，否则乘 sigmoid 因子。若为了计算 `exp` 使用稳定变换，仍保留 `bx_raw` 供谓词使用。把下界 clamp 后的量同时用于 false branch 和谓词，会改变分段语义；只有当前实现与独立 oracle 的 trigger/control 都支持时，它才是可证伪候选，不自动成为根因。

## 最小定位检查

- 选择 `bx` 位于阈值两侧、且两支预测输出不同的 control/trigger；先验证实际谓词使用的槽位和值，再比较 `gradInput`。两支恰好舍入为同一值时记录 `NO_DISCRIMINATION`，不能排除谓词错误。
- 对极负 `bx`，用当前任务冻结的 dtype 和逐步顺序计算 sigmoid 因子。若 actual 像 `gradOutput`、`sigmoid` 或另一个操作数，分别核对 `gradOutput/self/beta/threshold` 的实参槽位，而不是直接改公式或常量。
- 为每个候选值保留 `bx_raw`、稳定计算量、谓词输入、false-branch 结果和 `gradInput` 的最后 writer。CopyOut 必须从最后写入 `gradInput` 的槽位搬出；“VF 已写结果”不能证明搬出源正确。
- 比较 lower-clamp 开/关时，只改变 clamp，并保持槽位、谓词来源、dtype、CopyOut 和输入不变。只有输出按事前预测在 false branch 改变，才支持该候选；仍需证明 clamp 在当前合同中不应存在，才可归因。

## 使用范围

CANN 9.0.0 与 9.1.0 的 SoftplusBackward 产品合同均把 Ascend 950PR/950DT 标为该 ACLNN API 的“不支持”。本页可用于确认算法公式和参数角色；950PR 上的 API 支持、实现路径与数值模式必须另查当前版本的产品合同。
