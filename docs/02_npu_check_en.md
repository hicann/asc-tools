# npu_check

## Overview

The twin debugging provided by Ascend C Tools includes the debug function and the npu check function. The debug function covers aspects such as interface usage validation and parameter verification. On top of that, npu check provides memory checking, memory lifecycle management, memory address dependency management, and synchronization event management. Note that npu check only outputs complete verification logs and analysis when the debug phase exits normally (i.e., no ASSERT failures).

## Environment Preparation

Please refer to [Quick Start](00_quick_start.md) to complete the environment preparation.

## Usage

When operators developed with the Ascend C programming language are executed in the CPU domain via [cpu_debug](01_cpu_debug.md), the npu check tool simultaneously checks the operator implementation. The execution process and detected errors are saved as *_npuchk.log files in the npuchk folder under the execution path of the CPU-domain operator executable. Run the following command to generate the check results in one step:

  ``` bash
  # Without specifying a log file, the script automatically searches for log files in the current directory. git_clone_path is the clone path of this repository.
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py

  # Specify a log file
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/xxx_npuchk.log
  ```

- Errors detected: After the command finishes, failure results are displayed on screen. For example, error code ErrorRead3 and related failure information:

  ``` bash
  [V] [ErrorRead3] on read 0x7f328c11b010 0x800B
  Rule: Read out of bounds, length exceeds the actual valid data (start/end) allocated via Ascend C framework's alloc_buf
  ### vadd((__ubuf__ half*)7f328c11b810, (__ubuf__ half*)0xf328c11b010, (__ubuf__*)0x7f328c11b410, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)8, (uint8_t)8, (uint8_t)8);

  ---------------------- ERROR STATISTICS ----------------------
  1, ErrorRead3, Read out of bounds, length exceeds the actual valid data (start/end) allocated via Ascend C framework's alloc_buf
  ```

- No errors detected: Command completes with no screen output.

If errors are detected, you can view the detailed execution process in the log. Based on the log information, the following functional areas are covered.

### Anomaly Detection

npu check validates the legality of memory reads/writes, instruction synchronization, and tensor operations. Common failure types and their corresponding fields are as follows:

- **ErrorRead1:**
    Illegal memory read: The entire memory region was not allocated via Ascend C framework's AllocTensor or has already been freed by FreeTensor.
- **ErrorRead2:**
    [Suspicious] Reading invalid data: The memory being read was partially/entirely never written to, so the data may be invalid.
- **ErrorRead3:**
    Read out of bounds: The length exceeds the actual valid data (start/end) allocated via Ascend C framework's AllocTensor.
- **ErrorRead4:**
    Read address is not 32-byte aligned.
- **ErrorWrite1:**
    Illegal memory write: The memory was not allocated via Ascend C framework's AllocTensor or has already been freed by FreeTensor.
- **ErrorWrite2:**
    Write out of bounds: The length exceeds the actual valid data (start/end) allocated via Ascend C framework's AllocTensor.
- **ErrorWrite3:**
    [Suspicious] Duplicate write: The previously written memory has not been consumed, and is being overwritten.
- **ErrorWrite4:**
    Write address is not 32-byte aligned.
- **ErrorSync1:**
    Write synchronization issue: Missing pipe barrier within a pipe or missing set/wait between pipes.
- **ErrorSync2:**
    Read synchronization issue: Missing pipe barrier within a pipe or missing set/wait between pipes.
- **ErrorSync3:**
    set/wait pairing mismatch: Missing either set or wait.
- **ErrorSync4:**
    Duplicate eventID in set/wait operations, e.g., mte2:set0/set0, vector:set0/wait0.
- **ErrorLeak:**
    Memory leak: Memory was allocated but not freed.
- **ErrorFree:**
    Double free: Memory was already freed via free_buf, and free_buf is called again.
- **ErrorBuffer0:**
    Tensor memory was not initialized using Ascend C framework's InitBuffer.
- **ErrorBuffer1:**
    Tensor queue type is inconsistent with the type used during initialization.
- **ErrorBuffer2:**
    VECIN/VECOUT/VECCALC operations are non-compliant.
- **ErrorBuffer3:**
    Tensor operation memory is invalid. Possible causes: memory not allocated / memory out of bounds.
- **ErrorBuffer4:**
    TBufPool resource pool was not initialized using Ascend C framework's InitBufPool interface.

### EnQue/DeQue Error Scenario Check

For VECIN/VECOUT/VECCALC type Tensors, the tool checks whether a Tensor is in the correct state when it appears in a data transfer/compute instruction, to ensure synchronization correctness. Abnormal states are recorded in the log.

### GM Memory Multi-Core Conflict Check

Based on the GM global memory management mechanism, the tool records the GM address range operated by each core. If overlapping write address ranges are detected across multiple cores, an error is recorded. In Atomic add scenarios, overlapping addresses are not flagged as errors.

## Usage Example

Using the [add](https://gitcode.com/cann/asc-devkit/blob/master/examples/01_simd_cpp_api/01_utilities/06_cpu_debug/cpu_debug.asc) example, after calling the CPU debugging API and using gdb/printf to debug the operator kernel function, developers can use the npu check tool to check the kernel source code implementation logic based on the generated log file.

**Step 1**: Construct an error case

Add the following FreeTensor operation in the CopyIn function of the add_custom code.

``` cpp
AscendC::LocalTensor<half> xLocal = inQueueX.AllocTensor<half>();
AscendC::LocalTensor<half> yLocal = inQueueY.AllocTensor<half>();
// Add the following line here to construct an error example
inQueueX.FreeTensor(xLocal);
// The remaining code stays unchanged
AscendC::DataCopy(xLocal, xGm[progress * TILE_LENGTH], TILE_LENGTH);
AscendC::DataCopy(yLocal, yGm[progress * TILE_LENGTH], TILE_LENGTH);
inQueueX.EnQue(xLocal);
inQueueY.EnQue(yLocal);
```

Performing FreeTensor here will result in an illegal memory write.

**Step 2**: Use cpu debug to generate the log file

Refer to [cpu_debug](01_cpu_debug.md) and run the following commands to compile and generate the CPU-domain operator executable. The add_custom_x_x_npuchk.log file is saved in the npuchk folder under the newly created build folder in the execution path.

```bash
mkdir -p build && cd build;
cmake .. -DSOC_VERSION=${SOC_VERSION}; make -j
python3 ../scripts/gen_data.py
./add
python3 ../scripts/verify_result.py output_z.bin golden.bin
```

**Step 3**: Find the corresponding log file and run the check

Since this is a multi-core example, each core generates a separate log file. Taking core 0 as an example, the generated log file is add_custom_0_0_vec_npuchk.log. Run the following command to perform the check:

``` shell
python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/add_custom_0_0_vec_npuchk.log
```

  - If no xxx_npuchk.log file is specified, the script will automatically search for files with the "_npuchk.log" suffix in the current directory.

You can then view the stack trace information recorded by npu check in the log file.

**Step 4**: Determine the error type based on the screen output

When the example case has errors, the following information will be displayed:

``` shell
----------------------ERROR STATISTICS----------------------
1, ErrorBuffer2, VECIN/VECOUT/VECCALC operations are non-compliant
1, ErrorWrite1, Illegal memory write: Memory was not allocated via Ascend C framework's alloc_buf or has already been freed
```

You can then determine the error type based on the anomaly detection section above.
