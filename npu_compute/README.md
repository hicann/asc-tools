# NPU Compute

NPU Compute is a command-line profiling prototype with an injected collection
library and an ACLPTI kernel-replay implementation.

The product path is:

```text
npu-compute
  -> create a per-run staging directory
  -> launch target with ACL_API_INJECTION=libnpu-compute.so
     and NPU_COMPUTE_OUTPUT=<staging-directory>
  -> target Runtime initialization
  -> prof_api loads libnpu-compute.so
  -> acltoolInitialize
  -> ACLPTI subscription initializes hooks and replay dependencies
  -> Runtime callback enablement and section configuration
  -> hooked Runtime calls, shadow memory, and kernel replay
  -> first successful target Runtime EXIT triggers HardwareInfo collection
  -> Msprof raw callback and internal replay-data lifecycle
  -> CLI waitpid observes a successful target exit
  -> CLI validates HardwareInfo.jsonl and recursively packs the staging tree
  -> CLI atomically publishes a report_<epoch_ms>_<random_id>.npu-rep report
```

## Components

| Component | Integration artifact | Responsibility |
| --- | --- | --- |
| NPU Compute CLI | `npu-compute` | Validate options, launch and supervise the target, package collection files, and unpack imported reports |
| NPU Compute library | `libnpu-compute.so` | Export `acltoolInitialize`/`acltoolShutdown`, configure ACLPTI, collect HardwareInfo, and write profiling CSV files |
| ACLPTI | `libacl_pti.so` | Expand sections to PMU events, maintain shadow memory, and replay kernel launches |
| Prof API stub | `libprofapi.so` | Register the Runtime table, load the injection library, and simulate direct Msprof calls |
| Injection hook stub | `libacl_tool_injection.so` | Register API-shaped callbacks, install Runtime wrappers, and expose original Runtime functions |
| PTI data module | compiled into `libacl_pti.so` | Decode profiler chunks, aggregate task/PMU rows, and notify the registered shutdown handler after draining |
| Runtime stub | `libruntime.so` | Provide a complete local Runtime API table for integration tests |
| Demo application | `npu_compute_demo_app` | Exercise initialization, allocation, H2D initialization, kernel launch, and cleanup |

The prof API, injection hook, Runtime, and demo in this repository are
standalone integration stubs. The PTI replay-data implementation is compiled
into `libacl_pti.so` and used by NPU Compute through that library. These components
do not define a complete production framework.

ACLPTI's private C++ implementation is organized under
`npu_compute::aclpti::{callback,activity,data,profiling,runtime_replacement}`.
Only the cross-domain `Manager` remains in `npu_compute::aclpti`. Public
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
successful target Runtime EXIT starts the single collection attempt.
Repeated sections are deduplicated while preserving first-occurrence order.

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

Collection requires at least one `--section` and a target program. Arguments
after the target program are passed to the application unchanged. The CLI stays
as the parent process, forwards `SIGINT`, `SIGTERM`, and `SIGHUP` to the target
process group, reaps the child, and preserves normal or signal-derived exit
status. Each collection command receives a unique staging directory. If the
application exits successfully but `HardwareInfo.jsonl` is missing or is not a
regular file, the CLI reports the staging path and returns collection error 3.

For a collection command, `--export` selects the report destination. A path
ending in `.npu-rep` is the exact report file. An existing directory receives
an automatically named report. Without `--export`, the CLI creates the report
in the current directory:

```text
report_<epoch_ms>_<8-lowercase-hex-digits>.npu-rep
```

For an Import command, `--export` instead selects the output directory. The
directory must not already exist. Without `--export`, the CLI removes
`.npu-rep`, `.npu.rep`, or `.rep` from the input file name and creates that
directory under the current directory.

Examples:

```bash
# Collection: publish an automatically named report in the current directory.
npu-compute --section PipeUtilization ./application

# Collection: publish to the exact report path.
npu-compute --section PipeUtilization \
  --export result.npu-rep ./application

# Import: restore files to ./result/.
npu-compute --import result.npu-rep

# Import: restore files to the specified new directory.
npu-compute --import result.npu-rep --export restored-results
```

## Report Packaging And Import

After the target exits successfully, the CLI validates the collection output
and recursively packages the staging directory. Supported leaf files retain
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
npu-compute: staging=<absolute-staging-directory>
npu-compute: report=<absolute-report-path>
```

Import does not launch an application or initialize Runtime, ProfAPI, ACLPTI,
or `libnpu-compute.so`. The CLI validates the outer REP and every nested REP,
then restores leaf payloads without changing their bytes. A nested
`<name>.npu.rep` or `<name>.rep` entry becomes the directory `<name>`.

Import first writes into a private temporary directory beside the requested
destination. Leaf files are created exclusively without following symbolic
links and are synchronized before close. The complete directory is then
published with a no-replace rename. Existing output paths are never
overwritten, and a failed Import does not publish a partial final directory.
A successful Import prints:

```text
npu-compute: unpacked=<absolute-output-directory>
```

## HardwareInfo Collection

HardwareInfo collection is enabled by default and has no CLI switch. Do not
pass `HardwareInfo` to `--section`.

During `acltoolInitialize`, `libnpu-compute.so` subscribes an ACLPTI Runtime
callback and enables `aclrtSetDevice`, `aclrtMalloc`, and `aclrtLaunchKernel` in
that order. ACLPTI may report both ENTER and EXIT events. The library accepts
only a successful EXIT for one of those three APIs. The first accepted event
wakes a worker thread; later accepted events do not start another collection.

The worker collects host information and Device 0 information, then atomically
publishes `<staging-directory>/HardwareInfo.jsonl`. A successfully published
file contains five JSON objects in this order:

```jsonl
{"category":"Host Info","cpu physical count":0,"cpu logical count":0,"memory total size(MB)":0,"disk total size(GB)":0}
{"category":"Device Info","npu count":0,"chip info":"","arch info":""}
{"category":"CPU Information","control cpu count":0,"ai cpu count":0,"ai cpu frequency(MHZ)":0}
{"category":"AI Core Information","ai core count":0,"ai cube count":0,"ai vector count":0,"ai cube frequency(MHZ)":0,"ai vector frequency(MHZ)":0}
{"category":"Memory Information","hbm total(MB)":0,"hbm used(MB)":0,"hbm frequency(MHZ)":0}
```

This implementation supports a single-card collection and queries Device 0.
An individual device-field query failure is written to `stderr` with the
`[libnpu-compute] HardwareInfo:` prefix and leaves that field at its default
value. Initialization, host collection, serialization, or publication failure
also produces a diagnostic; when no final regular file is published, the CLI
returns collection error 3 and prints the retained staging directory.

## Build

The non-integration build creates `libnpu-compute.so` and the CLI without the
repository's local dependency implementations:

```bash
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_product
cmake --build /tmp/asc_tools_npu_compute_product -j2
```

This command verifies only those two artifacts. A runnable production stack
must provide an external complete `libacl_pti.so`, including the PTI data
module APIs. This configuration is therefore not a validated runnable
collection stack.

For a runnable local integration stack, explicitly enable the repository's
ACLPTI implementation and its ProfAPI, Injection Hook, and Runtime stubs
together with the tests:

```bash
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_integration \
  -DNPU_COMPUTE_BUILD_INTEGRATION_STUBS=ON \
  -DNPU_COMPUTE_BUILD_TESTS=ON
cmake --build /tmp/asc_tools_npu_compute_integration -j2
```

The asc-tools top-level build exposes the same switches:

```bash
cmake -S . -B build \
  -DASC_TOOLS_BUILD_NPU_COMPUTE=ON \
  -DNPU_COMPUTE_BUILD_INTEGRATION_STUBS=ON \
  -DNPU_COMPUTE_BUILD_TESTS=ON
```

## Run The Connected Demo

```bash
/tmp/asc_tools_npu_compute_integration/bin/npu-compute \
  --section PipeUtilization \
  /tmp/asc_tools_npu_compute_integration/bin/npu_compute_demo_app
```

Set `NPU_COMPUTE_DEBUG=1` to expose the Runtime registration, replacement
installation, shadow-memory operations, replay rounds, Msprof calls, and PTI
collection lifecycle:

```bash
NPU_COMPUTE_DEBUG=1 \
  /tmp/asc_tools_npu_compute_integration/bin/npu-compute \
    --section PipeUtilization \
    /tmp/asc_tools_npu_compute_integration/bin/npu_compute_demo_app
```

## Installation

Install the connected component into a staging prefix:

```bash
cmake --install /tmp/asc_tools_npu_compute_integration \
  --prefix /tmp/asc_tools_npu_compute_install \
  --component npu-compute-integration
```

The component installs:

```text
bin/npu-compute
lib64/libnpu-compute.so
lib64/libacl_pti.so
lib64/libprofapi.so
lib64/libacl_tool_injection.so
share/npu-compute/sections/
```

The demo application remains a build-tree test artifact.

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
