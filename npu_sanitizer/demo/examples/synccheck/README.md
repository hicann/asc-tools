# Synccheck 冒烟用例

本目录是一组 synccheck 端到端冒烟用例，用于快速确认从样例编译、设备插桩、callback
上报、同步配对分析、诊断渲染到会话结束的基本链路可用。每个同名子目录包含场景独有的
同名 `.asc`、`CMakeLists.txt`、`run.sh` 和 `verify.py`。父目录集中维护批量运行和结果校验逻辑，
公共工具和在线 DBI 源码由 `demo/build.sh` 统一准备。

`.asc` 只保留同步场景所需的设备指令，以及 ACL 初始化、kernel launch、stream 和清理逻辑。
正常路径与预期异常路径都属于冒烟看护范围；异常用例用于确认
synccheck 能稳定检出对应错误，不代表用例本身失效。

## 运行

先从仓库根目录统一构建公共工具：

```bash
bash npu_sanitizer/demo/build.sh
```

然后一次运行并统计全部冒烟用例：

```bash
bash npu_sanitizer/demo/examples/synccheck/run_all.sh
```

`run_all.sh` 使用已有的 `demo/build`，再依次执行全部用例。它保留各用例的原始输出，最后
打印逐项 `PASS/FAIL` 和 `total/passed/failed` 统计；任一用例失败时脚本返回 1。异常用例会打印
诊断信息；只要对应 `verify.py`、summary 和会话校验通过，runner 就返回 0 并输出
`example verification passed: synccheck/<用例名>`。

公共 runner 根据 `ASCEND_HOME_PATH` 加载 CANN 环境，并通过 `NPU_CHECK_DBI_ARCH` 和
`NPU_CHECK_DBI_TOOLCHAIN_ROOT` 控制在线 DBI 的公共架构和工具链。单独或批量执行用例时，
都可在命令前覆盖这些环境变量。

单独运行目标冒烟用例目录下的 `run.sh`：

```bash
bash npu_sanitizer/demo/examples/synccheck/single_pair/run.sh
bash npu_sanitizer/demo/examples/synccheck/single_unconsumed/run.sh
```

## 通过标准

- 正常用例：summary 与预期一致，`verify.py` 输出 `synccheck verification passed`，runner 返回 0。
- 异常用例：出现指定诊断且 summary 与预期一致，`verify.py` 输出
  `synccheck verification passed`，runner 返回 0。
- 构建失败、工具握手失败、会话未完整结束、诊断或计数不符，均视为冒烟用例失败。

## 冒烟矩阵

除无同步指令的 `no_sync` 外，纯 AIC 与纯 AIV 同步场景各 6 个；另外保留 1:1、1:2、2:1
三种 mix 比例用于后续能力回归。

| 冒烟用例 | 预期结果 | 看护场景 |
| --- | --- | --- |
| `no_sync/` | 正常 | 无同步指令时不产生 synccheck 误报。 |
| `single_pair/` | 正常 | AIC M 流水中匹配的 `SET_FLAG -> WAIT_FLAG` 能被正确消费。 |
| `single_unconsumed/` | 异常 | 会话结束时检出未消费的 `SET_FLAG`。 |
| `duplicate_set/` | 异常 | 检出 AIC M 流水中相同 key 的重复 `SET_FLAG`。 |
| `multi_block_isolation/` | 异常 | SET/WAIT 状态按 block 隔离，仅报告未被等待的 block。 |
| `wait_without_set/` | 异常 | 检出没有对应 `SET_FLAG` 的 `WAIT_FLAG`。 |
| `aic_wait_without_set/` | 异常 | AIC 的 M 流水检出没有对应 `SET_FLAG` 的 `WAIT_FLAG`。 |
| `mix_wait_without_set/` | 当前应 FAIL | `__mix__(1, 1)` 的 AIC M 与 AIV V 分支均执行无 SET 的 WAIT。 |
| `mutex_pair/` | 正常 | MTE2 和 V 流水中的 `GET_BUF -> RLS_BUF` 均能正确配对。 |
| `mutex_unreleased/` | 异常 | 会话结束时检出未释放的 `GET_BUF`。 |
| `aic_mutex_unreleased/` | 异常 | 会话结束时检出 AIC M 流水中未释放的 `GET_BUF`。 |
| `mix_mutex_unreleased/` | 当前应 FAIL | `__mix__(1, 2)` 的 AIC M 与 AIV V 分支均执行未释放的 `GET_BUF`。 |
| `mutex_unlock_without_lock/` | 异常 | 在 AIC M 流水中检出没有对应 `GET_BUF` 的 `RLS_BUF`。 |
| `mutex_duplicate_lock/` | 异常 | 检出相同 mutex ID 的重复阻塞式 `GET_BUF`。 |
| `mutex_multi_block_isolation/` | 异常 | AIC M 流水的 GET/RLS 状态按 block 隔离，仅报告未释放的 block。 |

## 看护约束

- 每个冒烟用例必须保留同名 `.asc`、独立 `CMakeLists.txt`、可执行 `run.sh` 和 `verify.py`；
  `run.sh` 必须在用例自身的 `build/` 中构建 target、保存日志并运行；父目录只维护批量运行和
  校验逻辑。
- `.asc` 必须在触发点前用 `// Scenario:` 简述被看护场景，并包含完整的主机侧运行上下文。
- `verify.py` 必须明确声明预期 summary 和诊断类型，不能只检查日志中是否出现某个字符串。
- `wait_without_set`、`aic_wait_without_set`、`mix_wait_without_set` 和 `mutex_duplicate_lock`
  会使设备指令阻塞，必须使用有界 stream 同步和 `aclrtDestroyStreamForce`，保证冒烟执行能够结束。
- `demo/tests/check_synccheck_examples_layout.sh` 看护所有冒烟包的目录契约、脚本语法和文档矩阵；
  修改同步事件翻译、checker、renderer 或运行链后，应至少执行受影响用例，并以 `single_pair`
  作为基本正常链路冒烟用例。
