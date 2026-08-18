#include "options.h"
#include "process_runner.h"

#include <iostream>

int main(int argc, char** argv)
{
    npu::sanitizer::cli::Options options{};
    std::string error;
    if (!npu::sanitizer::cli::ParseOptions(argc, argv, options, error)) {
        std::cerr << "npu_check: " << error << "\n\n" << npu::sanitizer::cli::Usage();
        return 64;
    }
    if (options.showHelp) {
        std::cout << npu::sanitizer::cli::Usage();
        return 0;
    }
    std::string libraryPath;
    if (!npu::sanitizer::cli::ResolveLibraryPath(options.libraryPath, libraryPath, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return 64;
    }
    return npu::sanitizer::cli::RunApplication(options, libraryPath);
}
