#!/bin/bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -e

run_command() {
    local cmd="$*"
    echo "Executing command: $cmd"

    if ! output=$("$@" 2>&1); then
         local exit_code=$?
         echo -e "\nCommand execution failed!"
         echo -e "\nFailed command: $cmd"
         echo -e "\nError output: $output"
         echo -e "\nExit code: $exit_code"
         exit $exit_code
    fi
}

version_ge() {
    # Version comparison, format: xx.xx.xx
    IFS='.' read -r -a curr_arr <<< "$1"
    IFS='.' read -r -a req_arr <<< "$2"

    for ((i=0; i<${#req_arr[@]}; i++)); do
        curr=${curr_arr[i]:-0}
        req=${req_arr[i]}
        if (( curr > req )); then
            return 0
        elif (( curr < req )); then
            return 1
        fi
    done
    return 0
}

detect_os() {
    # OS detection, supports debian (uses apt), rhel (uses dnf or yum)
    if [[ "$(uname -s)" == "Linux" ]]; then
        if [[ -f /etc/debian_version ]]; then
            OS="debian"
            PKG_MANAGER="apt"
        elif [[ -f /etc/redhat-release ]]; then
            OS="rhel"
            if command -v dnf &> /dev/null; then
                PKG_MANAGER="dnf"
            else
                PKG_MANAGER="yum"
            fi
        else
            echo "Unsupported Linux version"
            exit 1
        fi
    else
        echo "Unsupported OS type"
        exit 1
    fi
}

detect_usr() {
    # Root users don't need sudo for install commands.
    local curr_usr=''
    curr_usr=$(whoami)
    if [ "$curr_usr" = "root" ]; then
        SUDO=''
    else
        SUDO="sudo"
    fi
}

install_python() {
    # Python version >= 3.7.0
    echo -e "\n==== Checking Python ===="
    local req_ver="3.7.0"
    local curr_ver=""

    if command -v python3 &> /dev/null; then
        curr_ver=$(python3 --version 2>&1 | awk '{print $2}')
        echo "Current Python version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "Python version meets requirements"
            return
        fi
    fi
    echo "Installing Python..."
    case "$OS" in
        debian)
            run_command $SUDO $PKG_MANAGER update
            run_command $SUDO $PKG_MANAGER install -y python3 python3-pip python3-dev
            ;;
        rhel)
            if grep -q "release 7" /etc/redhat-release; then
                run_command $SUDO $PKG_MANAGER install -y centos-release-scl
                run_command $SUDO $PKG_MANAGER install -y rh-python38 rh-python38-python-devel
                run_command source /opt/rh/rh-python38/enable
                echo "Need to execute 'source /opt/rh/rh-python38/enable' to activate python3.8"
            else
                run_command $SUDO $PKG_MANAGER install -y python3 python3-pip python3-devel
            fi
            ;;
    esac

    if command -v python3 &> /dev/null; then
        curr_ver=$(python3 --version 2>&1 | awk '{print $2}')
        if version_ge "$curr_ver" "$req_ver"; then
            echo "Python installed successfully ($curr_ver)"
        else
            echo "Python version still doesn't meet requirements, please install manually"
            exit 1
        fi
    else
        echo "Python installation failed"
        exit 1
    fi
}

install_py_pkg() {
    local pkg_name=$1
    local req_ver=$2
    local curr_ver=""

    echo -e "\n==== Checking $pkg_name ===="


    curr_ver=$(pip3 show "$pkg_name" 2>&1 | awk 'NR == 2 {print $2}')
    if [[ -n "$curr_ver" ]]; then
        echo "Current $pkg_name version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "$pkg_name version meets requirements"
            return
        fi
    fi

    echo "Installing $pkg_name..."
    run_command pip3 install $pkg_name

    curr_ver=$(pip3 show "$pkg_name" 2>&1 | awk 'NR == 2 {print $2}')
    if [[ -n "$curr_ver" ]]; then
        echo "$pkg_name installed successfully ($curr_ver)"
    else
        echo "$pkg_name version still doesn't meet requirements, please install manually"
        exit 1
    fi
}

install_gcc() {
    # GCC version >= 7.3.0
    echo -e "\n==== Checking GCC ===="
    local req_ver="7.3.0"
    local curr_ver=""

    if command -v gcc &> /dev/null; then
        curr_ver=$(gcc --version | awk '/^gcc/ {print $4}')
    elif command -v g++ &> /dev/null; then
        curr_ver=$(g++ --version | awk '/^g\+\+/ {print $4}')
    else
        curr_ver="0.0.0"
    fi
    echo "Current GCC version: $curr_ver"
    if version_ge "$curr_ver" "$req_ver"; then
        echo "GCC version meets requirements ($curr_ver)"
        return
    fi

    echo "Installing GCC..."
    case "$OS" in
        debian)
            run_command $SUDO $PKG_MANAGER update
            run_command $SUDO $PKG_MANAGER install -y gcc-9 g++-9
            run_command $SUDO update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90 \
                --slave /usr/bin/g++ g++ /usr/bin/g++-9
            ;;
        rhel)
            if grep -q "release 7" /etc/redhat-release; then
                run_command $SUDO $PKG_MANAGER install -y centos-release-scl
                run_command $SUDO $PKG_MANAGER install -y devtoolset-9-gcc devtoolset-9-gcc-c++
                run_command source /opt/rh/devtoolset-9/enable
                echo "Need to execute 'source /opt/rh/devtoolset-9/enable' to activate GCC9"
            else
                run_command $SUDO $PKG_MANAGER install -y gcc gcc-c++
            fi
            ;;
    esac

    if command -v gcc &> /dev/null; then
        curr_ver=$(gcc --version | awk '/^gcc/ {print $4}')
        if version_ge "$curr_ver" "$req_ver"; then
            echo "GCC installed successfully ($curr_ver)"
        else
            echo "GCC version still doesn't meet requirements, please install manually."
            exit 1
        fi
    else
        echo "GCC installation failed"
        exit 1
    fi
}

install_cmake() {
    # CMake version >= 3.16.0
    echo -e "\n==== Checking CMake ===="
    local req_ver="3.16.0"
    local curr_ver=""

    if command -v cmake &> /dev/null; then
        curr_ver=$(cmake --version | awk '/^cmake/ {print $3}')
        echo "Current CMake version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "CMake meets requirements"
            return
        fi
    fi

    echo "Installing CMake..."
    case "$OS" in
        debian)
            if grep -q "Ubuntu 18.04" /etc/os-release; then
                run_command wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
                run_command echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ bionic main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
                run_command $SUDO apt update
                run_command $SUDO apt install -y cmake
            else
                run_command $SUDO $PKG_MANAGER update
                run_command $SUDO $PKG_MANAGER install -y cmake
            fi
            ;;
        rhel)
            if grep -q "release 7" /etc/redhat-release; then
                run_command $SUDO $PKG_MANAGER install -y epel-release
                run_command $SUDO $PKG_MANAGER install -y cmake3
                run_command $SUDO ln -s /usr/bin/cmake3 /usr/bin/cmake
            else
                run_command $SUDO $PKG_MANAGER install -y cmake
            fi
            ;;
    esac

    if command -v cmake &> /dev/null; then
        curr_ver=$(cmake --version | awk '/^cmake/ {print $3}')
        if version_ge "$curr_ver" "$req_ver"; then
            echo "CMake installed successfully ($curr_ver)"
        else
            echo "CMake version still doesn't meet requirements, please install manually"
            exit 1
        fi
    else
        echo "CMake installation failed"
        exit 1
    fi
}

install_ccache() {
    # ccache version >= 4.8.2
    echo -e "\n==== Checking ccache ===="
    local req_ver="4.8.2"
    local curr_ver=""

    if command -v ccache &> /dev/null; then
        curr_ver=$(ccache --version 2>&1 | head -n 1 | awk '{print $3}')
        echo "Current ccache version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "ccache meets requirements"
            return
        fi
    fi

    echo "Installing ccache..."
    case "$OS" in
        debian|rhel)
            apt_ver=$(apt list caache -a 2>&1 | awk 'NR ==5 {print $2}')
            if version_ge "$req_ver" "$apt_ver"; then
                echo "The ccache version provided by apt is too old. Please install it manually:"
                echo "1. Download source: wget https://github.com/ccache/ccache/releases/download/v4.8.2/ccache-4.8.2.tar.gz"
                echo "2. Extract and compile: tar -zxf ccache-4.8.2.tar.gz && cd ccache-4.8.2 && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make && sudo make install"
                exit 1
            fi
            run_command $SUDO $PKG_MANAGER install -y ccache
            ;;
    esac

    if command -v ccache &> /dev/null; then
        curr_ver=$(ccache --version 2>&1 | head -n 1 | awk '{print $2}')
        echo "ccache installed successfully ($curr_ver)"
    else
        echo "ccache installation failed, can be ignored"
    fi
}

install_pigz() {
    # pigz version >= 2.4
    echo -e "\n==== Checking pigz ===="
    local req_ver="2.4"
    local curr_ver=""

    if command -v pigz &> /dev/null; then
        curr_ver=$(pigz --version 2>&1 | awk '{print $2}')
        echo "Current pigz version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "pigz meets requirements"
            return
        fi
    fi

    read -p "Install pigz? [Y/n] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Skipping pigz installation"
        return
    fi

    echo "Installing pigz..."
    case "$OS" in
        debian|rhel)
            run_command $SUDO $PKG_MANAGER install -y pigz
            ;;
    esac

    if command -v pigz &> /dev/null; then
        curr_ver=$(pigz --version 2>&1 | awk '{print $2}')
        echo "pigz installed successfully ($curr_ver)"
    else
        echo "pigz installation failed, can be ignored"
    fi
}

install_googletest() {
    # Recommended googletest version: release-1.11.0
    echo -e "\n==== Checking googletest ===="
    local req_ver="1.11.0"
    local curr_ver=""
    local gtest_src_dir="/usr/src/gtest"

    if pkg-config --exists gtest; then
        curr_ver=$(pkg-config --modversion gtest)
        echo "Current googletest version: $curr_ver"
        if version_ge "$curr_ver" "$req_ver"; then
            echo "googletest meets requirements"
            return
        fi
    fi

    echo "Installing googletest..."
    case "$OS" in
        debian)
            # Install libgtest-dev
            msg_1="1. Download source: wget https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz"
            msg_2="2. Extract and compile: tar -zxf release-1.11.0.tar.gz && cd googletest-release-1.11.0 && cmake . && make && sudo make install"
            
            apt_ver=$(apt list libgtest-dev -a 2>&1 | awk 'NR == 5 {print $2}')
            if version_ge "$req_ver" "$apt_ver"; then
                echo "The gtest version provided by apt is too old. Please install it manually:"
                echo "$msg_1"
                echo "$msg_2"
                exit 1
            fi

            run_command $SUDO $PKG_MANAGER install -y libgtest-dev
            # Check if gtest source directory exists
            if [ ! -d "$gtest_src_dir" ]; then
                echo "googletest source directory not found: $gtest_src_dir"
                echo "Attempting to reinstall libgtest-dev..."
                run_command $SUDO $PKG_MANAGER purge -y libgtest-dev
                run_command $SUDO $PKG_MANAGER install -y libgtest-dev
                # Check directory again
                if [ ! -d "$gtest_src_dir" ]; then
                    echo "Still cannot find $gtest_src_dir, please install manually:"
                    echo "$msg_1"
                    echo "$msg_2"
                    exit 1
                fi
            fi
            # Force cmake execution in gtest source directory (even if cd fails)
            echo "Entering $gtest_src_dir to compile..."
            run_command $SUDO cmake -S "$gtest_src_dir" -B "$gtest_src_dir/build"
            run_command $SUDO make -C "$gtest_src_dir/build"
            run_command $SUDO cp "$gtest_src_dir/build/lib/"*.a /usr/lib
            ;;
        rhel)
            run_command $SUDO $PKG_MANAGER install -y gtest gtest-devel
            ;;
    esac

    if pkg-config --exists gtest; then
        curr_ver=$(pkg-config --modversion gtest)
        echo "googletest installed successfully ($curr_ver)"
    else
        echo "googletest installation failed"
        exit 1
    fi
}

install_lcov() {
    # lcov version >= 1.13
    echo -e "\n==== Checking lcov ===="
    if command -v lcov &> /dev/null; then
        echo "lcov has been installed"
        return
    fi

    echo "Installing lcov..."
    case "$OS" in
        debian|rhel)
            run_command $SUDO $PKG_MANAGER install -y lcov
            ;;
    esac

    if command -v lcov &> /dev/null; then
        echo "lcov installed successfully"
    else
        echo "lcov installation failed, please install it manually."
        exit 1
    fi
}

main() {
    echo "===================================================="
    echo "Starting project dependency installation"
    echo "===================================================="

    detect_os
    install_python
    install_py_pkg pytest "5.4.2"
    install_py_pkg pytest-cov "2.8.1"
    install_gcc
    install_cmake
    install_ccache
    install_pigz
    install_googletest
    install_lcov

    echo -e "===================================================="
    echo "All dependencies installed successfully!"
    echo "===================================================="
}

main "$@"