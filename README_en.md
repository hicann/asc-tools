# Ascend C Tools

## 🚀 Overview

Ascend C Tools is a debugging toolkit provided by [CANN](https://hiascend.com/software/cann) (Compute Architecture for Neural Networks) based on the [Ascend C](https://gitcode.com/cann/asc-devkit) programming language. With Ascend C Tools, developers can perform CPU-domain twin debugging, parse operator debugging information and file information, thereby quickly locating potential issues in operator implementations.

- **cpu debug**

    The cpu debug tool essentially provides a CPU debugging library that enables Ascend C source code to be compiled with a standard GCC compiler into operator binaries that can run and be debugged on the CPU. This tool assists developers in performing basic functional and accuracy verification on the CPU, and provides debugging methods such as GDB debugging and printf output.

- **npu check**

    The npu check tool is used to inspect the implementation logic of Kernel source code. Its features include: memory checking, multi-thread checking, memory lifecycle management, memory address dependency management, and synchronization event management.

- **msobjdump**

    msobjdump provides parsing and extraction capabilities for operator ELF (Executable and Linkable Format) files compiled from Kernel direct-invocation operator development and engineered operator development, presenting the resulting information in a readable format so that developers can intuitively obtain Kernel file information.

- **show_kernel_debug_data**

    The show_kernel_debug_data tool is used for offline parsing of Kernel-side operator debugging information saved via the AscendC::DumpTensor/AscendC::print interfaces.


## 🔍 Directory Structure

The directory structure of this repository is as follows:

```
├── cmake                               // Ascend C Tools build source code
├── cpudebug                            // Ascend C Tools cpu debug tool implementation source code
│   ├── cmake                           // Ascend C Tools cpu debug build source code
│   ├── include                         // Ascend C Tools cpu debug tool implementation source code
│   ├── utils                           // Ascend C Tools cpu debug tool implementation source code
│   └── src                             // Ascend C Tools cpu debug tool implementation source code
├── docs                                // Ascend C Tools usage documentation
├── examples                            // Ascend C Tools example projects
├── libraries                           // Library files that Ascend C Tools depends on
├── npuchk                              // Ascend C Tools npu check inspection tool
├── scripts                             // Ascend C Tools packaging scripts
├── tests                                // Ascend C Tools UT test cases
├── third_party                         // Third-party library files that Ascend C Tools depends on
├── utils
│   ├── msobjdump                       // Ascend C Tools msobjdump implementation source code
└── └── show_kernel_debug_data          // Ascend C Tools show_kernel_debug_data implementation source code
```

## 📖 Documentation

| Document | Description |
|------|------|
|[Quick Start](./docs/00_quick_start.md)|A quick tutorial to get started with the project. Includes environment setup, compilation and execution, and local verification.|
|[Usage Guide](./docs)|Usage instructions for each tool.|
|[Related Documentation](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)|Ascend C operator programming guide, which also provides detailed introductions to the twin debugging cpu_debug, msobjdump, and show_kernel_debug_data tools.|


## 📝 Related Information

- [Contribution Guide](CONTRIBUTING.md)
- [Security Statement](SECURITY.md)
- [License](LICENSE)
