# NPU Compute

NPU Compute is a command-line profiling prototype with an injected collection
library and an ACLPTI kernel-replay implementation.

The product path is:

```text
npu-compute
  -> create a per-run collection data directory in the current working directory
  -> launch target with ACL_API_INJECTION=libnpu-compute.so
     and NPU_COMPUTE_OUTPUT=<data-directory>
  -> target Runtime initialization
  -> prof_api loads libnpu-compute.so
  -> acltoolInitialize
  -> ACLPTI subscription initializes hooks and replay dependencies
  -> Runtime callback enablement and section configuration
  -> hooked Runtime calls, shadow memory, and kernel replay
  -> first successful target kernel-launch Runtime EXIT triggers HardwareInfo collection
  -> Msprof raw callback and internal replay-data lifecycle
  -> CLI waitpid observes a successful target exit
  -> CLI validates HardwareInfo.jsonl and recursively packs the collection data directory
  -> CLI atomically publishes a report_<epoch_ms>_<random_id>.npu-rep report
```

## Components

| Component | Integration artifact | Responsibility |
| --- | --- | --- |
| NPU Compute CLI | `npu-compute` | Validate options, launch and supervise the target, package collection files, and unpack imported reports |
| NPU Compute library | `libnpu-compute.so` | Export `acltoolInitialize`/`acltoolShutdown`, configure ACLPTI, collect HardwareInfo, and write profiling CSV files |
| ACLPTI | `libacl_pti.so` | Expand sections to PMU events, maintain shadow memory, and replay kernel launches |
| CANN Prof API | `libprofapi.so` | Register the Runtime table, load the injection library, and receive profiling data |
| Injection Hook | `libacl_tool_injection.so` | Register API replacements and expose original Runtime functions |
| PTI data module | compiled into `libacl_pti.so` | Decode profiler chunks, aggregate task/PMU rows, and notify the registered shutdown handler after draining |
| CANN Runtime | `libacl_rt.so`, `libruntime.so` | Provide Runtime APIs and API injection dispatch |

The production build uses the Prof API and Runtime libraries from the configured
CANN package. The Prof API and Runtime implementations in this repository are
for unit tests only. The PTI replay-data implementation is compiled into
`libacl_pti.so`, and NPU Compute uses the implementation through that library.

ACLPTI's private C++ implementation is organized under
`npu_compute::aclpti::{callback,activity,data,profiling,replacement}`.
Only the cross-domain initialization module remains in `npu_compute::aclpti`. Public
`aclptiXxx` APIs stay in the global namespace, and file-local helpers stay in
anonymous namespaces.

## Supported Sections

The CLI exposes the section catalog currently backed by CSV writers:

```text
PipeUtilization
Memory
MemoryL0
MemoryUB
L2Cache
```

`HardwareInfo` is not a section. Its collection is enabled by default for every
collection command and is independent of the selected PMU sections. The first
successful target kernel-launch Runtime EXIT starts the single collection
attempt. Repeated sections are deduplicated while preserving first-occurrence
order.

## CLI

```text
npu-compute [options] [program] [program-arguments]
```

Public options:

```text
-h, --help
    --section <id>
    --list-sections
    --replay-mode kernel
-i, --import <repo>
-o, --export <repo>
```

When an exact `-h` or `--help` appears in the `[options]` region, the CLI prints
help and exits with status 0. Help takes precedence over other tool options and
their validation in that region. Arguments after the target program belong to
the application, so `-h` or `--help` there is passed to the application unchanged.

Collection requires at least one `--section` and a target program. Arguments
after the target program are passed to the application unchanged. The CLI stays
as the parent process, forwards `SIGINT`, `SIGTERM`, and `SIGHUP` to the target
process group, reaps the child, and preserves normal or signal-derived exit
status. Each collection command receives a unique collection data directory in
the command's current working directory. If the application exits successfully
but `HardwareInfo.jsonl` is missing or is not a regular file, the CLI returns
collection error 3. At the end of collection, an empty collection data directory
is removed without reporting its path. A directory containing partial collection
files is retained and reported.

A target program must not start another `npu-compute` collection. When nested
collection is detected, neither the inner nor the outer command publishes a REP.
The outer command returns collection error 3 and prints:

```text
npu-compute: nested npu-compute collection is not supported
```

Help, Section listing, and REP import commands are not collection commands.

For a collection command, `--export` selects the report destination. A path
ending in `.npu-rep` is the exact report file. An existing directory receives
an automatically named report. Without `--export`, the CLI creates the report
in the current directory:

```text
report_<epoch_ms>_<8-lowercase-hex-digits>.npu-rep
```

For an Import command, `--export` selects an existing directory. Each import
creates a unique result subdirectory in that directory. Without `--export`,
the CLI creates a unique result subdirectory in the current directory. The
result directory name has the format
`npu-compute-import-<milliseconds>-<process-id>-<random-suffix>`.

Examples:

```bash
# Collection: publish an automatically named report in the current directory.
npu-compute --section PipeUtilization ./application

# Collection: publish to the exact report path.
npu-compute --section PipeUtilization \
  --export result.npu-rep ./application

# Import: create a unique result directory in the current directory.
npu-compute --import result.npu-rep

# Import: create a unique result directory in an existing directory.
mkdir restored-results
npu-compute --import result.npu-rep --export restored-results
```

## Report Packaging And Import

After the target exits successfully, the CLI validates the collection output
and recursively packages the collection data directory. Supported leaf files retain
their original names and bytes. Every child directory is encoded as a
`type=NpuRep` entry named `<directory>.npu.rep`; that payload is a complete
nested REP with offsets starting from zero in its own byte space. This rule is
applied recursively without a fixed depth limit.

The packer accepts collection files with `.json`, `.jsonl`, `.csv`,
`.sqlite3`, `.pb`, and `.protobuf` suffixes. It excludes
`.hardware_info.lock`, rejects remaining temporary files and symbolic links,
and validates JSONL and CSV completeness before reading their payloads.

The report writer creates a temporary file in the report's destination
directory, writes and synchronizes the complete REP, reads it back to verify
the bytes and layout, then publishes it with a no-replace rename and
synchronizes the destination directory. Existing report files are never
overwritten. A successful collection prints both retained diagnostic paths:

```text
npu-compute: data-directory=<absolute-data-directory>
npu-compute: report=<absolute-report-path>
```

Import does not launch an application or initialize Runtime, ProfAPI, ACLPTI,
or `libnpu-compute.so`. The CLI validates the outer REP and every nested REP,
then restores leaf payloads without changing their bytes. A nested
`<name>.npu.rep` or `<name>.rep` entry becomes the directory `<name>`.

Import first writes into a private temporary directory in the parent directory
of the result subdirectory. Leaf files are created exclusively without
following symbolic links and are synchronized before close. The complete
directory is then published as the result subdirectory with a no-replace
rename. Existing files are never overwritten, and a failed Import does not
publish a partial result directory. A successful Import prints:

```text
npu-compute: unpacked=<absolute-output-directory>
```

## HardwareInfo Collection

HardwareInfo collection is enabled by default and has no CLI switch. Do not
pass `HardwareInfo` to `--section`.

During `acltoolInitialize`, `libnpu-compute.so` subscribes an ACLPTI Runtime
callback and enables `aclrtLaunchKernel` and `aclrtLaunchKernelWithHostArgs` in
that order. ACLPTI may report both ENTER and EXIT events. The library accepts
only a successful EXIT for one of those two APIs. The callback thread for the
first accepted event collects host information and Device information in that
order, serializes the result, and atomically publishes
`<data-directory>/HardwareInfo.jsonl`. The callback returns after publication
completes or the collection enters the failed state.

If other threads in the same target process enter an accepted callback while
collection is in progress, they wait until that collection completes or fails
and do not collect again. Shutdown first disables the enabled callbacks and
waits for an in-progress collection to reach a final state. If no valid Kernel
EXIT callback is received, `HardwareInfo.jsonl` is not generated. A successfully
published file contains five JSON objects in this order:

```jsonl
{"category":"Host Info","cpu physical count":0,"cpu logical count":0,"memory total size(MB)":0,"disk total size(GB)":0}
{"category":"Device Info","npu count":0,"chip info":"","arch info":""}
{"category":"CPU Information","control cpu count":0,"ai cpu count":0,"ai cpu frequency(MHZ)":0}
{"category":"AI Core Information","ai core count":0,"ai cube count":0,"ai vector count":0,"ai cube frequency(MHZ)":0,"ai vector frequency(MHZ)":0}
{"category":"Memory Information","hbm total(MB)":0,"hbm used(MB)":0,"hbm frequency(MHZ)":0}
```

This implementation supports a single-card collection and queries Device.
An individual device-field query failure is written to `stderr` with the
`[libnpu-compute] HardwareInfo:` prefix and leaves that field at its default
value. Initialization, host collection, serialization, or publication failure
also produces a diagnostic; when no final regular file is published, the CLI
returns collection error 3. A collection data directory containing partial files
is retained and reported.

## Build

Load the target CANN package environment before building the production
artifacts. Use the following commands for the current test environment:

```bash
source /home/chenning/AscendEnv/test_profiling/cann-9.2.0/set_env.sh
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_product \
  -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build /tmp/asc_tools_npu_compute_product -j2
```

The build derives the CANN architecture package from the header location under
`ASCEND_HOME_PATH`. Set `NPUCOMPUTE_CANN_ROOT` only to override that environment.
Set `CMAKE_ASC_ARCHITECTURES` to match the target NPU (`dav-2201` or `dav-3510`).
The build generates `libnpu-compute.so`, `libacl_pti.so`,
`libacl_tool_injection.so`, and the `npu-compute` CLI. It links the Prof API and
Runtime from CANN instead of the repository stubs.

Unit tests automatically use the Prof API and Runtime stubs from this
repository. The Runtime stub only overrides the injection declarations. Its
`#include_next` still requires CANN headers from the loaded environment:

```bash
source /home/chenning/AscendEnv/test_profiling/cann-9.2.0/set_env.sh
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_ut \
  -DNPU_COMPUTE_BUILD_TESTS=ON
cmake --build /tmp/asc_tools_npu_compute_ut -j2
LD_LIBRARY_PATH=/tmp/asc_tools_npu_compute_ut/bin:${LD_LIBRARY_PATH} \
  ctest --test-dir /tmp/asc_tools_npu_compute_ut --output-on-failure
```

The asc-tools top-level build provides the NPU Compute test switch:

```bash
cmake -S . -B build \
  -DASC_TOOLS_BUILD_NPU_COMPUTE=ON \
  -DNPU_COMPUTE_BUILD_TESTS=ON
```

## Installation

Build the default asc-tools run package from the repository root:

```bash
bash build.sh --pkg
```

Install the generated package to the default CANN path:

```bash
./build_out/cann-asc-tools_<version>_linux-<arch>.run --full --pylocal
```

Inside the asc-tools run package, architecture-dependent files use the CANN
architecture directory. Here, `<arch>` is the value reported by `uname -m`:

```text
<arch>-linux/bin/npu-compute
<arch>-linux/lib64/libnpu-compute.so
<arch>-linux/lib64/libacl_pti.so
<arch>-linux/lib64/libacl_tool_injection.so
<arch>-linux/include/aclpti/*.h
```

During installation, CANN exposes the architecture-dependent directories
through the top-level `bin`, `lib64`, and `include` symbolic links. After
running `source <install-root>/cann/set_env.sh`, the public installed paths are:

```text
$ASCEND_HOME_PATH/bin/npu-compute
$ASCEND_HOME_PATH/lib64/libnpu-compute.so
$ASCEND_HOME_PATH/lib64/libacl_pti.so
$ASCEND_HOME_PATH/lib64/libacl_tool_injection.so
$ASCEND_HOME_PATH/include/aclpti/*.h
```

The matching base CANN Runtime package provides `libprofapi.so` and
`libacl_rt.so` under the same public `lib64` path. The ProfAPI and Runtime stubs
remain build-tree test artifacts and are not installed by the asc-tools run
package.

## Replay Contract

ACLPTI mirrors each successful device allocation with a same-sized shadow
allocation. Hooked H2D/D2D memcpy and memset operations update the shadow. A
free releases both allocations.

ACLPTI registers complete replacement functions for malloc, free, memcpy,
memset, and kernel launch. Each replacement calls the original Runtime function
through `acltoolGetOriginalRuntimeApi`; ACLPTI updates shadow state or starts
replay only after that original call succeeds.

For each successful original kernel launch API, ACLPTI splits the selected PMU
events into rounds of at most ten values. Each round restores shadow state,
prepares an internal replay record, starts Msprof, calls the corresponding
original launch API, synchronizes the stream, stops Msprof, records collection
replay status, and releases the replay. The final successful round leaves the
application-visible buffer in its post-kernel state, then shuts down the PTI
data module after every round has completed. An error also shuts down the data
module before `ReplayKernel` returns, while preserving the original error code.

This initial implementation supports one replayed kernel launch per process.
Later kernel launches cannot start another replay after the data module has
shut down. The data module decodes and aggregates profiler records
asynchronously, but the system does not track initialized subranges, snapshot
multi-kernel sequences, persist profiler output, serialize concurrent
collection, or compensate for a failed later round.
