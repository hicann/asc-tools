/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file op_custom_registry_generator.cpp
 * \brief
 */

#include "ascendc_tool_log.h"
#include "op_custom_registry_generator.h"

namespace ops {
void OpCustomGenerator::OpCustomGenDestryFunc(std::ofstream& outfile) const
{
    outfile << "__attribute__((destructor)) void DestroyCustomOpRegistry()\n";
    outfile << "{\n";
    outfile << "    std::string basePath = std::string(basePathChar);\n";
    outfile << "    size_t lastSlashPos = (basePath).rfind('/');\n";
    outfile << "    size_t secondLastSlashPos = (basePath).rfind('/', lastSlashPos - 1u);\n";
    outfile << "    std::string oppBasePath = (basePath).substr(0, secondLastSlashPos);\n";
    outfile << "    if (!RemoveDirectoryRecursively(oppBasePath)) {\n";
    outfile << "        LOG_ERROR(\"Failed to remove directory: %s\", oppBasePath.c_str());\n";
    outfile << "    } else {\n";
    outfile << "        LOG_INFO(\"Successfully removed directory: %s\", oppBasePath.c_str());\n";
    outfile << "        const std::string oppSubstring = \"/opp/\";\n";
    outfile << "        size_t pos = oppBasePath.find(oppSubstring);\n";
    outfile << "        std::string oppDir = oppBasePath.substr(0, pos + oppSubstring.length() - 1);\n";
    outfile << "        if (IsDirectoryEmpty(oppDir)) {\n";
    outfile << "            if (rmdir(oppDir.c_str()) != 0) {\n";
    outfile << "                LOG_WARN(\"Failed to remove empty opp directory: %s\", oppDir.c_str());\n";
    outfile << "            } else {\n";
    outfile << "                LOG_INFO(\"Successfully removed empty opp directory: %s\", oppDir.c_str());\n";
    outfile << "            }\n";
    outfile << "        }\n";
    outfile << "    }\n";
    outfile << "}\n";
}

void OpCustomGenerator::OpCustomGen(std::ofstream& outfile, const char* vendorName) const
{
    outfile << "REGISTER_OP_LIB(";
    outfile << vendorName;
    outfile << ").RegOpLibInit(ImplCustomOpRegistry);\n\n";
    outfile << "bool IsDirectoryEmpty(const std::string& dirPath) {\n";
    outfile << "    DIR* dir = opendir(dirPath.c_str());\n";
    outfile << "    if (dir == nullptr) {\n";
    outfile << "        const int32_t currentErr = errno;\n";
    outfile << "        if (currentErr == ENOENT) {\n";
    outfile << "            LOG_WARN(\"Directory is already empty: %s\", dirPath.c_str());\n";
    outfile << "            return true;\n";
    outfile << "        } else {\n";
    outfile << "            LOG_ERROR(\"Failed to open directory: %s\", dirPath.c_str());\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "    }\n";
    outfile << "    dirent* entry = readdir(dir);\n";
    outfile << "    while (entry != nullptr) {\n";
    outfile << "        if (strncmp(entry->d_name, \".\", 1) != 0 && strncmp(entry->d_name, \"..\", 2) != 0) {\n";
    outfile << "            CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "        entry = readdir(dir);\n";
    outfile << "    }\n";
    outfile << "    CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "    return true;\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenUtilsFunc(std::ofstream& outfile) const
{
    outfile << "char basePathChar[PATH_MAX];\n\n";
    outfile << "void CheckCloseDir(DIR* dir, const char* file, int line, const char* func)\n";
    outfile << "{\n";
    outfile << "    if (closedir(dir) != 0) {\n";
    outfile << "        LOG_ERROR(\"closedir failed, [%s:%d]%s\", file, line, func);\n";
    outfile << "    }\n";
    outfile << "}\n\n";
    outfile << "bool FileExist(const std::string& filename)\n";
    outfile << "{\n";
    outfile << "    std::ifstream file(filename);\n";
    outfile << "    return file.good();\n";
    outfile << "}\n\n";
    outfile << "bool DirectoryExists(const std::string& path)\n";
    outfile << "{\n";
    outfile << "    DIR* dir = opendir(path.c_str());\n";
    outfile << "    if (dir != nullptr) {\n";
    outfile << "        CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "        return true;\n";
    outfile << "    } else {\n";
    outfile << "        return false;\n";
    outfile << "    }\n";
    outfile << "}\n\n";
    outfile << "std::string GetCurrentTimestamp()\n";
    outfile << "{\n";
    outfile << "    std::time_t now = std::time(nullptr);\n";
    outfile << "    char buf[64];\n";
    outfile << "    (void)std::strftime(buf, sizeof(buf), \"%Y%m%d_%H%M%S\", std::localtime(&now));\n";
    outfile << "    return std::to_string(getpid()) + \"_\" + std::string(buf);\n";
    outfile << "}\n\n";
    outfile << "bool CheckWritePermission(const std::string& path)\n";
    outfile << "{\n";
    outfile << "    if (access(path.c_str(), W_OK) != 0) {\n";
    outfile << "        LOG_ERROR(\"No write permission for the path: %s, errno: %d, error: %s\", path.c_str(), errno, strerror(errno));\n";
    outfile << "        return false;\n";
    outfile << "    }\n";
    outfile << "    return true;\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenGetBasePathFunc(std::ofstream& outfile) const
{
    outfile << "bool GetBasePath()\n";
    outfile << "{\n";
    outfile << "    const char *ascendWorkPathEnv = std::getenv(\"ASCEND_WORK_PATH\");\n";
    outfile << "    std::string currentTimestamp = GetCurrentTimestamp();\n";
    outfile << "    std::string path;\n";
    outfile << "    if (ascendWorkPathEnv != nullptr) {\n";
    outfile << "        char resolvedPath[PATH_MAX];\n";
    outfile << "        if (realpath(ascendWorkPathEnv, resolvedPath) == nullptr) {\n";
    outfile << "            LOG_ERROR(\"Invalid ASCEND_WORK_PATH: %s\", ascendWorkPathEnv);\n";
    outfile << "            return false;\n";
    outfile << "        } else {\n";
    outfile << "            std::string ascendWorkPath = std::string(resolvedPath);\n";
    outfile << "            path = ascendWorkPath + \"/opp/\" + currentTimestamp + \"/vendors/\";\n";
    outfile << "            if (!CheckWritePermission(ascendWorkPath)) {\n";
    outfile << "                return false;\n";
    outfile << "            }\n";
    outfile << "        }\n";
    outfile << "    } else {\n";
    outfile << "        path = \"/tmp/opp/\" + currentTimestamp + \"/vendors/\";\n";
    outfile << "        if (!CheckWritePermission(\"/tmp/\")) {\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "    }\n";
    outfile << "    errno_t err = strcpy_s(basePathChar, PATH_MAX, path.c_str());\n";
    outfile << "    if (err != 0) {\n";
    outfile << "        LOG_ERROR(\"Error copying string, error code: %d\", err);\n";
    outfile << "        return false;\n";
    outfile << "    }\n";
    outfile << "    return true;\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenWirteFileFunc(std::ofstream& outfile) const
{
    outfile << "bool CreateDirectory(const std::string& path)\n";
    outfile << "{\n";
    outfile << "    if (DirectoryExists(path)) {\n";
    outfile << "        return true;\n";
    outfile << "    } else {\n";
    outfile << "        char* p = const_cast<char*>(path.c_str());\n";
    outfile << "        char* slash = strchr(p + 1, '/');\n";
    outfile << "        while (slash != nullptr) {\n";
    outfile << "            *slash = '\\0';\n";
    outfile << "            int status = mkdir(p, S_IRWXU | S_IRWXG);\n";
    outfile << "            *slash = '/';\n";
    outfile << "            if (status == -1 && errno != EEXIST) {\n";
    outfile << "                LOG_ERROR(\"Error create diectory failed\");\n";
    outfile << "                return false;\n";
    outfile << "            }\n";
    outfile << "            slash = strchr(slash + 1, '/');\n";
    outfile << "        }\n";
    outfile << "        int status = mkdir(p, S_IRWXU | S_IRWXG);\n";
    outfile << "        if (status == -1 && errno != EEXIST) {\n";
    outfile << "            LOG_ERROR(\"Error create diectory failed, path: %s\", path.c_str());\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "        return true;\n";
    outfile << "    }\n";
    outfile << "}\n\n";
    outfile << "void WriteBinaryFile(const std::string& path, const uint8_t* start, const uint8_t* end)\n";
    outfile << "{\n";
    outfile << "    std::ofstream outfile(path, std::ios::binary);\n";
    outfile << "    if (!outfile) {\n";
    outfile << "        LOG_ERROR(\"Error opening file for writing: %s\", path.c_str());\n";
    outfile << "        return;\n";
    outfile << "    }\n";
    outfile << "    std::copy(start, end, std::ostreambuf_iterator<char>(outfile));\n";
    outfile << "    if (!outfile) {\n";
    outfile << "        LOG_ERROR(\"Error writing file: %s\", path.c_str());\n";
    outfile << "    }\n";
    outfile << "    LOG_INFO(\"Successfully writing file: %s\", path.c_str());\n";
    outfile << "}\n\n";
    outfile << "std::string GetParentPath(const std::string& path)\n";
    outfile << "{\n";
    outfile << "    size_t pos = path.find_last_of(\"/\\\\\");\n";
    outfile << "    if (pos == std::string::npos) {\n";
    outfile << "        return \"\";\n";
    outfile << "    }\n";
    outfile << "    return path.substr(0, pos);\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenSymlinkFunc(std::ofstream& outfile) const
{
    outfile << "bool CreateSymlink(const std::string& targetPath, const std::string& linkPath)\n";
    outfile << "{\n";
    outfile << "    try {\n";
    outfile << "        if (!FileExist(targetPath)) {\n";
    outfile << "            throw std::runtime_error(\"Target file does not exist: \" + targetPath);\n";
    outfile << "        }\n";
    outfile << "        std::string parentDir = GetParentPath(linkPath);\n";
    outfile << "        if (!DirectoryExists(parentDir)) {\n";
    outfile << "            if (!CreateDirectory(parentDir)) {\n";
    outfile << "                LOG_ERROR(\"Failed create directory: %s\", parentDir.c_str());\n";
    outfile << "                return false;\n";
    outfile << "            }\n";
    outfile << "        }\n";
    outfile << "        if (symlink(targetPath.c_str(), linkPath.c_str()) != 0) {\n";
    outfile << "            LOG_ERROR(\"Error creating symlink failed.\");\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "        LOG_INFO(\"Successfully creating symlink: %s\", linkPath.c_str());\n";
    outfile << "        return true;\n";
    outfile << "    } catch (const std::exception& e) {\n";
    outfile << "        LOG_ERROR(\"Error creating symlink: %s\", e.what());\n";
    outfile << "        return false;\n";
    outfile << "    }\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenSymArchFunc(std::ofstream& outfile) const
{
    outfile << "std::string GetSystemArchitecture() {\n";
    outfile << "    const char* systemArch = std::getenv(\"SYSTEM_PROCESSOR\");\n";
    outfile << "    if (systemArch != nullptr) {\n";
    outfile << "        return std::string(systemArch);\n";
    outfile << "    } else {\n";
    outfile << "        struct utsname sysInfo;\n";
    outfile << "        if (uname(&sysInfo) == 0) {\n";
    outfile << "            return sysInfo.machine;\n";
    outfile << "        } else {\n";
    outfile << "            return \"\";\n";
    outfile << "        }\n";
    outfile << "    }\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenRegistryfirstFunc(std::ofstream& outfile) const
{
    outfile << "uint32_t ImplCustomOpRegistry(ge::AscendString& op_lib_path)\n";
    outfile << "{\n";
    outfile << "    if (!GetBasePath()) {\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string basePath = std::string(basePathChar);\n";
    outfile << "    if (!CreateDirectory(basePath)) {\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string vendorName = \"\";\n";
    outfile << "    for (const auto& fileInfo : AscendC::__ascendc_op_info) {\n";
    outfile << "        std::string fileName = std::get<0>(fileInfo).GetString();\n";
    outfile << "        std::string filePath = std::get<1>(fileInfo).GetString();\n";
    outfile << "        const uint8_t* start = std::get<2>(fileInfo);\n";
    outfile << "        const uint8_t* end = std::get<3>(fileInfo);\n";
    outfile << "        std::string fullPath = (basePath) + \"/\" + filePath + \"/\" + fileName;\n";
    outfile << "        if (!CreateDirectory((basePath) + \"/\" + filePath)) {\n";
    outfile << "            LOG_ERROR(\"Failed to create subdirectory: %s\", filePath.c_str());\n";
    outfile << "            return 1;\n";
    outfile << "        }\n";
    outfile << "        WriteBinaryFile(fullPath, start, end);\n";
    outfile << "        if (vendorName.empty()) {\n";
    outfile << "            size_t firstSlashPos = filePath.find('/');\n";
    outfile << "            if (firstSlashPos != std::string::npos) {\n";
    outfile << "                vendorName = filePath.substr(0, firstSlashPos);\n";
    outfile << "            } else {\n";
    outfile << "                LOG_ERROR(\"Failed to get vendor_name\");\n";
    outfile << "                return 1;\n";
    outfile << "            }\n";
    outfile << "        }\n";
    outfile << "    }\n";
}
void OpCustomGenerator::OpCustomGenRegistrylastFunc(std::ofstream& outfile) const
{
    outfile << "    op_lib_path = ConvertToAscendString(basePath + vendorName);\n";
    outfile << "    Dl_info dlInfo;\n";
    outfile << "    if (!dladdr((void*)&ImplCustomOpRegistry, &dlInfo)) {\n";
    outfile << "        LOG_ERROR(\"dladdr failed: %s\", dlerror());\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string targetPath = dlInfo.dli_fname;\n";
    outfile << "    char resolvedPath[PATH_MAX];\n";
    outfile << "    if (realpath(targetPath.c_str(), resolvedPath) == nullptr) {\n";
    outfile << "        LOG_ERROR(\"Failed to resolve libcust_opapi.so path: %s\", strerror(errno));\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    targetPath = std::string(resolvedPath);\n";
    outfile << "    LOG_INFO(\"Resolve libcust_opapi.so path is: %s\", targetPath.c_str());\n";
    outfile << "    std::string arch = GetSystemArchitecture();\n";
    outfile << "    if (arch.empty()) {\n";
    outfile << "        LOG_ERROR(\"Failed to get system architecture name\");\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string opmasterPath = basePath + vendorName + \"/op_impl/ai_core/tbe/op_tiling/lib/linux/\" +\n";
    outfile << "        arch + \"/libcust_opmaster_rt2.0.so\";\n";
    outfile << "    if (!CreateSymlink(targetPath, opmasterPath)) {\n";
    outfile << "        LOG_ERROR(\"Failed to create symlink for libcust_opmaster_rt2.0.so\");\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string opsprotoPath = basePath + vendorName + \"/op_proto/lib/linux/\" + arch + \"/libcust_opsproto_rt2.0.so\";\n";
    outfile << "    if (!CreateSymlink(targetPath, opsprotoPath)) {\n";
    outfile << "        LOG_ERROR(\"Failed to create symlink for libcust_opsproto_rt2.0.so\");\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    std::string optilingPath = basePath + vendorName + \"/op_impl/ai_core/tbe/op_tiling/liboptiling.so\";\n";
    outfile << "    if (!CreateSymlink(targetPath, optilingPath)) {\n";
    outfile << "        LOG_ERROR(\"Failed to create symlink for liboptiling.so\");\n";
    outfile << "        return 1;\n";
    outfile << "    }\n";
    outfile << "    return 0;\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenRemoveDirFunc(std::ofstream& outfile) const
{
    outfile << "bool RemoveDirectoryRecursively(const std::string& path)\n";
    outfile << "{\n";
    outfile << "    DIR* dir = opendir(path.c_str());\n";
    outfile << "    if (dir == nullptr) {\n";
    outfile << "        const int32_t currentErr = errno;\n";
    outfile << "        if (currentErr == ENOENT) {\n";
    outfile << "            LOG_WARN(\"Directory does not exist: %s\", path.c_str());\n";
    outfile << "            return true;\n";
    outfile << "        } else {\n";
    outfile << "            LOG_ERROR(\"Failed to open directory: %s\", path.c_str());\n";
    outfile << "            return false;\n";
    outfile << "        }\n";
    outfile << "    }\n";
    outfile << "    dirent* entry = readdir(dir);\n";
    outfile << "    while (entry != nullptr) {\n";
    outfile << "        std::string entryPath = path + \"/\" + entry->d_name;\n";
    outfile << "        if (static_cast<int32_t>(entry->d_type) == DT_DIR) {\n";
    outfile << "            if (std::string(entry->d_name) != \".\" && std::string(entry->d_name) != \"..\") {\n";
    outfile << "                if (!RemoveDirectoryRecursively(entryPath)) {\n";
    outfile << "                    LOG_ERROR(\"Failed to remove subdirectory: %s\", entryPath.c_str());\n";
    outfile << "                    CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "                    return false;\n";
    outfile << "                }\n";
    outfile << "                LOG_INFO(\"Successfully remove subdirectory: %s\", entryPath.c_str());\n";
    outfile << "            }\n";
    outfile << "        } else {\n";
    outfile << "            if (unlink(entryPath.c_str()) != 0) {\n";
    outfile << "                LOG_ERROR(\"Failed to unlink file: %s\", entryPath.c_str());\n";
    outfile << "                CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "                return false;\n";
    outfile << "            }\n";
    outfile << "            LOG_INFO(\"Successfully remove linkpath: %s\", entryPath.c_str());\n";
    outfile << "        }\n";
    outfile << "        entry = readdir(dir);\n";
    outfile << "    }\n";
    outfile << "    CheckCloseDir(dir, __FILE__, __LINE__, __FUNCTION__);\n";
    outfile << "    bool result = (rmdir(path.c_str()) == 0);\n";
    outfile << "    return result;\n";
    outfile << "}\n\n";
}

void OpCustomGenerator::OpCustomGenMacro(std::ofstream& outfile) const
{
    outfile << "#define ASCENDC_MODULE_NAME static_cast<int32_t>(ASCENDCKERNEL)\n";
    outfile
        << "#define LOG_ERROR(format, ...)                                                                                                   \\\n";
    outfile
        << "    do {                                                                                                                         \\\n";
    outfile
        << "        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_ERROR) == 1) {                                                         \\\n";
    outfile
        << "            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_ERROR, \"[%s] \" format \"\\n\", __FUNCTION__, ##__VA_ARGS__);      \\\n";
    outfile
        << "        }                                                                                                                        \\\n";
    outfile << "    } while (0)\n";
    outfile
        << "#define LOG_WARN(format, ...)                                                                                                    \\\n";
    outfile
        << "    do {                                                                                                                         \\\n";
    outfile
        << "        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_WARN) == 1) {                                                          \\\n";
    outfile
        << "            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_WARN, \"[%s] \" format \"\\n\", __FUNCTION__, ##__VA_ARGS__);       \\\n";
    outfile
        << "        }                                                                                                                        \\\n";
    outfile << "    } while (0)\n";
    outfile
        << "#define LOG_INFO(format, ...)                                                                                                    \\\n";
    outfile
        << "    do {                                                                                                                         \\\n";
    outfile
        << "        if (AlogCheckDebugLevel(ASCENDC_MODULE_NAME, DLOG_INFO) == 1) {                                                          \\\n";
    outfile
        << "            AlogRecord(ASCENDC_MODULE_NAME, DLOG_TYPE_DEBUG, DLOG_INFO, \"[%s] \" format \"\\n\", __FUNCTION__, ##__VA_ARGS__);       \\\n";
    outfile
        << "        }                                                                                                                        \\\n";
    outfile << "    } while (0)\n";
}

void OpCustomGenerator::OpCustomGenHead(std::ofstream& outfile) const
{
    outfile << "#include <iostream>\n";
    outfile << "#include <fstream>\n";
    outfile << "#include <vector>\n";
    outfile << "#include <tuple>\n";
    outfile << "#include <cstring>\n";
    outfile << "#include <cstdint>\n";
    outfile << "#include <ctime>\n";
    outfile << "#include <sstream>\n";
    outfile << "#include <sys/stat.h>\n";
    outfile << "#include <sys/utsname.h>\n";
    outfile << "#include <dirent.h>\n";
    outfile << "#include <unistd.h>\n";
    outfile << "#include <dlfcn.h>\n";
    outfile << "#include \"ge_table_op_resource.h\"\n";
    outfile << "#include \"graph/ascend_string.h\"\n";
    outfile << "#include \"graph/operator_reg.h\"\n";
    outfile << "#include \"register/op_lib_register.h\"\n";
    outfile << "#include \"register/op_impl_registry.h\"\n";
    outfile << "#include \"base/alog_pub.h\"\n\n";
}

opbuild::Status OpCustomGenerator::GenerateCode(void)
{
    ASCENDLOGI("CustomOpRegistry GenerateCode called!");
    auto vendorName = std::getenv("ASCEND_VENDOR_NAME");
    if (vendorName == nullptr || std::string(vendorName) == std::string("")) {
        ASCENDLOGI("vendor_name is null");
        return opbuild::OPBUILD_SUCCESS;
    }
    std::string genPath;
    Generator::GetGenPath(genPath);
    char resolvedGenPath[PATH_MAX] = {0};
    if (realpath(genPath.c_str(), resolvedGenPath) == nullptr) {
        std::cerr << "[Error]: Path: " << genPath << " is not valid!" << std::endl;
        ASCENDLOGE("Generate Path %s is invalid!", genPath.c_str());
        return opbuild::OPBUILD_FAILED;
    }
    fileGenPath = std::string(resolvedGenPath);
    std::ofstream opCustomCpp(fileGenPath + "/" + "custom_op_registry" + ".cpp");
    OpCustomGenHead(opCustomCpp);
    OpCustomGenMacro(opCustomCpp);
    OpCustomGenUtilsFunc(opCustomCpp);
    OpCustomGenGetBasePathFunc(opCustomCpp);
    OpCustomGenWirteFileFunc(opCustomCpp);
    OpCustomGenSymlinkFunc(opCustomCpp);
    OpCustomGenSymArchFunc(opCustomCpp);
    OpCustomGenRegistryfirstFunc(opCustomCpp);
    OpCustomGenRegistrylastFunc(opCustomCpp);
    OpCustomGenRemoveDirFunc(opCustomCpp);
    OpCustomGen(opCustomCpp, vendorName);
    OpCustomGenDestryFunc(opCustomCpp);
    opCustomCpp.close();
    ASCENDLOGI("CustomOpRegistry GenerateCode end!");
    return opbuild::OPBUILD_SUCCESS;
}

OpCustomGenerator::OpCustomGenerator(std::vector<std::string>& ops) : Generator(ops)
{
    ASCENDLOGI("CustomOpRegistry Generator construct!");
}

static opbuild::Status OpCustomGeneratorBuilder(std::vector<std::string>& ops)
{
    OpCustomGenerator g(ops);
    return g.GenerateCode();
}

static void AddOpCustomGenerator(void) __attribute__((constructor));
void AddOpCustomGenerator(void)
{
    GeneratorFactory::AddBuilder("custom_op_registry", OpCustomGeneratorBuilder);
}
}