# msobjdump

## Overview
This tool provides parsing and extraction capabilities for generated operator ELF files (Executable and Linkable Format), presenting the results in a readable format so that developers can intuitively obtain kernel file information. For a detailed introduction of this tool, please refer to the "Programming Guide > Appendix > msobjdump Tool" section in [Ascend C Operator Development](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC).

For a tool usage demonstration, refer to the [msobjdump example](../examples/04_msobjdump/README.md).

## Command Format

- Command to parse an ELF file

    ```bash
    msobjdump --dump-elf <elf_file> [--verbose]
    ```

    --dump-elf <elf_file> is required and specifies the path of the ELF file to parse. [--verbose] is optional and enables full printing of device information in the ELF file.

- Command to extract an ELF file

    ```bash
    msobjdump --extract-elf <elf_file> [--out-dir <out_path>]
    ```

    --extract-elf <elf_file> is required and specifies the path of the ELF file to parse. [--out-dir <out_path>] is optional and sets the output path for extracted files.

- Command to list ELF file contents

    ```bash
    msobjdump --list-elf <elf_file>
    ```

    --list-elf <elf_file> is optional. It retrieves the list of device information files contained in the ELF file and prints it.

  The following table describes common fields in ELF files:

| Field Name | Description | Required | Print Behavior |
| ---- | ---- | ---- | ---- |
| `.ascend.meta. ${id}` | Represents the operator kernel function name, where `${id}` is the index of the meta information. | Yes | Printed by default without `--verbose`. |
| `VERSION` | Represents the version number. | Yes | Printed by default without `--verbose`. |
| `DEBUG` | Debug-related information, consisting of two parts:<br>`debugBufSize`: Memory space required for debug information.<br>`debugOptions`: Debug switch status. Values:<br>`0`: Debug switch off.<br>`1`: Debugging via DumpTensor and printf.<br>`2`: Debugging via assert.<br>`4`: Debugging via timestamp marking.<br>`8`: Debugging via memory out-of-bounds detection. | No | Printed by default without `--verbose`. |
| `DYNAMIC_PARAM` | Whether the operator kernel function uses dynamic parameters. Values:<br>`0`: Dynamic parameter mode off.<br>`1`: Dynamic parameter mode on. | No | Printed by default without `--verbose`. |
| `OPTIONAL_PARAM` | Optional parameter information, consisting of two parts:<br>`optionalInputMode`: Whether optional inputs require placeholders in the operator kernel function.<br>`0`: Optional inputs do not occupy placeholders.<br>`1`: Optional inputs occupy placeholders.<br>`optionalOutputMode`: Whether optional outputs require placeholders in the operator kernel function.<br>`0`: Optional outputs do not occupy placeholders.<br>`1`: Optional outputs occupy placeholders. | No | Printed by default without `--verbose`. |
| `KERNEL_TYPE` | Represents the core type used at kernel function runtime. | No | Printed by default without `--verbose`. |
| `CROSS_CORE_SYNC` | Represents the hardware synchronization syncall type.<br>`USE_SYNC`: Use hardware synchronization.<br>`NO_USE_SYNC`: Do not use hardware synchronization. | No | Printed by default without `--verbose`. |
| `MIX_TASK_RATION` | Represents the Cube core/Vector core ratio allocation type during kernel function execution. | No | Printed by default without `--verbose`. |
| `DETERMINISTIC_INFO` | Indicates whether the operator uses deterministic computation.<br>`0`: Non-deterministic computation.<br>`1`: Deterministic computation. | No | Printed by default without `--verbose`. |
| `BLOCK_NUM` | Represents the number of execution cores. This field is currently not supported and only prints the default value `0xFFFFFFFF`. | No | Printed by default without `--verbose`. |
| `FUNCTION_ENTRY` | The value of the operator TilingKey. | No | Printed by default without `--verbose`. |
| `elf header infos` | Includes ELF Header, Section Headers, Key to Flags, Program Headers, Symbol table, and other information. | No | Set `--verbose` to enable full printing. |
