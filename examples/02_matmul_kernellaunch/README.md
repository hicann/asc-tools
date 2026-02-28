<!--声明：本文使用[Creative Commons License version 4.0](https://creativecommons.org/licenses/by/4.0/legalcode)许可协议，转载、引用或修改等操作请遵循此许可协议。-->
# Matmul算子样例  

## 概述
本样例介绍Matmul算子的核函数直调方法。

## 目录结构介绍
```
└── 02_matmul_kernellaunch          // 使用核函数直调的方式调用Matmul自定义算子。
    └── MatmulInvocationNeo         // Kernel Launch方式调用核函数样例。
```

## 算子描述
Matmul高阶API实现了快速的Matmul矩阵乘法的运算操作。

Matmul的计算公式为：

```
C = A * B
```

- A、B为源操作数，A为左矩阵，形状为\[M, K]；B为右矩阵，形状为\[K, N]。
- C为目的操作数，存放矩阵乘结果的矩阵，形状为\[M, N]。

## 算子规格描述
在核函数直调样例中，算子实现支持的shape为：M = 512, N = 1024, K = 512。
<table>
<tr><td rowspan="1" align="center">算子类型(OpType)</td><td colspan="4" align="center">Matmul</td></tr>
</tr>
<tr><td rowspan="3" align="center">算子输入</td><td align="center">name</td><td align="center">shape</td><td align="center">data type</td><td align="center">format</td></tr>
<tr><td align="center">a</td><td align="center">M * K</td><td align="center">float16</td><td align="center">ND</td></tr>
<tr><td align="center">b</td><td align="center">K * N</td><td align="center">float16</td><td align="center">ND</td></tr>
</tr>
<tr><td rowspan="1" align="center">算子输出</td><td align="center">c</td><td align="center">M * N</td><td align="center">float</td><td align="center">ND</td></tr>
</tr>
<tr><td rowspan="1" align="center">核函数名</td><td colspan="4" align="center">matmul_custom</td></tr>
</table>

## 支持的AI处理器
- Ascend 910C
- Ascend 910B

## 编译运行样例算子
详细操作请参考[MatmulInvocationNeo样例运行](./MatmulInvocationNeo/README.md)。

## 更新说明
| 时间       | 更新事项                 |
| ---------- | ------------------------ |
| 2025/11/11 | 新增readme               |
