# CPU Debug Direct-Invocation Example

## Overview

This example demonstrates the Ascend C CPU Debug debugging workflow based on the Add operator. In CPU Debug mode, the example kernel runs in the CPU domain, allowing the use of GDB to set breakpoints, step through execution, and inspect call stacks and memory state.

## Supported Products and CANN Software Versions

| Product | CANN Software Version |
|------|-------------|
| Ascend 950PR/Ascend 950DT | >= CANN 9.1.0 |
| Atlas A3 Training Series Products/Atlas A3 Inference Series Products | >= CANN 9.0.0 |
| Atlas A2 Training Series Products/Atlas A2 Inference Series Products | >= CANN 9.0.0 |

## Directory Structure

```shell
├── 02_cpudebug
│   ├── CMakeLists.txt      // Build project file
│   ├── add.asc             // Ascend C operator implementation & invocation example
│   └── README.md           // Example documentation
```

## Example Description

- Example functionality:

  The Add computation formula is:

  ```shell
  z = x + y
  ```

- Example specifications:
  <table border="2" align="center">
  <caption>Table 1: Add Example Specification Description</caption>
  <tr><td rowspan="1" align="center">Example Type (OpType)</td><td colspan="4" align="center">Add</td></tr>
  <tr><td rowspan="3" align="center">Example Input</td><td align="center">name</td><td align="center">shape</td><td align="center">data type</td><td align="center">format</td></tr>
  <tr><td align="center">x</td><td align="center">[8, 2048]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td align="center">y</td><td align="center">[8, 2048]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">Example Output</td><td align="center">z</td><td align="center">[8, 2048]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">Kernel Function Name</td><td colspan="4" align="center">add_custom</td></tr>
  </table>

- Example implementation:

  The example uses a fixed shape of `[8, 2048]`. Input data is first transferred from Global Memory to Local Memory, vector addition is performed via `AscendC::Add`, and the result is then transferred back from Local Memory to Global Memory. The computation flow is divided into three stages: `CopyIn`, `Compute`, and `CopyOut`.

## Build and Run

Execute the following steps in the root directory of this example to build and run it.

- Configure environment variables

  Please configure environment variables according to the [installation method](../../docs/00_quick_start.md#prepare&install) of the CANN development toolkit on your current environment.

  ```bash
  source ${install_path}/cann/set_env.sh
  ```

  > **Note:** `${install_path}` is the CANN package installation directory. When no installation directory is specified, the default installation path is `/usr/local/Ascend`.

- Run the example

  Execute the following commands in this example directory.

  ```bash
  mkdir -p build && cd build;
  cmake -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=dav-2201 ..;make -j;
  ./add
  ```

- Build option description

  | Option | Values | Description |
  |------|--------|------|
  | `CMAKE_ASC_RUN_MODE` | `cpu` | Run mode: CPU debug |
  | `CMAKE_ASC_ARCHITECTURES` | `dav-2201` (default), `dav-3510` | NPU architecture:<br>&bull; dav-2201, corresponding to Atlas A2 Training Series Products/Atlas A2 Inference Series Products and Atlas A3 Training Series Products/Atlas A3 Inference Series Products<br>&bull; dav-3510, corresponding to Ascend 950PR/Ascend 950DT |

- Execution result

  The execution result is as follows, indicating that the accuracy comparison succeeded.

  ```bash
  [Success] Case accuracy is verification passed.
  ```

## GDB Debugging

Execute the following command in the `build` directory to start GDB debugging.

```bash
gdb --args ./add
```

After entering GDB, you can set breakpoints, step through execution, or inspect the call stack as needed.
