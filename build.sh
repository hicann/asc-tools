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

SUPPORTED_SHORT_OPTS=("h" "j" "t" "p")
SUPPORTED_LONG_OPTS=(
  "help" "cov" "cache" "pkg" "asan" "make_clean" "cann_3rd_lib_path" "test" "cann_path"
)

CURRENT_DIR=$(dirname $(readlink -f ${BASH_SOURCE[0]}))
BUILD_DIR=${CURRENT_DIR}/build
OUTPUT_DIR=${CURRENT_DIR}/build_out
USER_ID=$(id -u)
CPU_NUM=$(($(cat /proc/cpuinfo | grep "^processor" | wc -l)))
THREAD_NUM=16
CUSTOM_OPTION="-DCMAKE_INSTALL_PREFIX=${OUTPUT_DIR} -DBUILD_OPEN_PROJECT=ON -DCMAKE_BUILD_TYPE=Debug"
CANN_3RD_LIB_PATH=${CURRENT_DIR}/third_party
DEPS_FILE_PATH=${CURRENT_DIR}/third_party

dotted_line="----------------------------------------------------------------"

usage() {
  local specific_help="$1"

  if [[ -n "$specific_help" ]]; then
    case "$specific_help" in
      package)
        echo "Package Build Options:"
        echo $dotted_line
        echo "    --pkg                Compile run package"
        echo "    -p, --cann_path      Set the cann package installation directory, eg: /usr/local/Ascend/latest"
        echo "    -j                   Compile thread nums, default is 16, eg: -j 8"
        echo "    --cann_3rd_lib_path  Set the path for third-party library dependencies, eg: ./build"
        echo "    --asan               Enable ASAN (address Sanitizer)"
        echo $dotted_line
        echo "Examples:"
        echo "    bash build.sh --pkg -j 8"
        echo "    bash build.sh --pkg --asan -j 32"
        return
        ;;
      test)
        echo "Test Options:"
        echo $dotted_line
        echo "    -t, --test           Build and run all unit tests"
        echo "    -p, --cann_path      Set the cann package installation directory, eg: /usr/local/Ascend/latest"
        echo "    -j                   Compile thread nums, default is 16, eg: -j 8"
        echo "    --cann_3rd_lib_path  Set the path for third-party library dependencies, eg: ./build"
        echo "    --cov                Enable code coverage for unit tests"
        echo "    --asan               Enable ASAN (address Sanitizer)"
        echo $dotted_line
        echo "Examples:"
        echo "    bash build.sh -t --cov"
        echo "    bash build.sh --test_part --asan -j 32"
        return
        ;;
      clean)
        echo "Clean Options:"
        echo $dotted_line
        echo "    --make_clean         Clean build artifacts"
        echo $dotted_line
        echo "Examples:"
        echo "    bash build.sh -t --make_clean"
        return
        ;;
    esac
  fi

  echo "build script for asc-devkit repository"
  echo "Usage: bash build.sh [OPTION]..."
  echo ""
  echo "    The following are all supported arguments:"
  echo $dotted_line
  echo "    -h, --help           Display help information"
  echo "    -j                   Compile thread nums, default is 16, eg: -j 8"
  echo "    -t, --test           Build and run all unit tests"
  echo "    -p, --cann_path      Set the cann package installation directory, eg: /usr/local/Ascend/latest"
  echo "    --pkg                Compile run package"
  echo "    --cann_3rd_lib_path  Set the path for third-party library dependencies, eg: ./build"
  echo "    --cov                Enable code coverage for unit tests"
  echo "    --asan               Enable ASAN (address Sanitizer)"
  echo "    --make_clean         Clean build artifacts"
}

function log() {
  local current_time=`date +"%Y-%m-%d %H:%M:%S"`
  echo "[$current_time] "$1
}

function copy_deps_file() {
  if ls ${BUILD_DIR}/simulator*.tar.gz 1> /dev/null 2>&1; then
    cp -r ${BUILD_DIR}/simulator*.tar.gz ${DEPS_FILE_PATH}
  fi

  if ls ${BUILD_DIR}/cpudebug_deps*.tar.gz 1> /dev/null 2>&1; then
    cp -r ${BUILD_DIR}/cpudebug_deps*.tar.gz ${DEPS_FILE_PATH}
  fi
}

function clean()
{
  if [ -n "${BUILD_DIR}" ];then
    rm -rf ${BUILD_DIR}
  fi

  if [ -n "${OUTPUT_DIR}" ];then
    rm -rf ${OUTPUT_DIR}
  fi

  mkdir -p ${BUILD_DIR} ${OUTPUT_DIR}
}

check_option_validity() {
  local arg="$1"

  if [[ "$arg" =~ "=" ]]; then
    arg="${arg%%=*}"
  fi

  if [[ "$arg" =~ ^-[^-] ]]; then
    if [[ ! " ${SUPPORTED_SHORT_OPTS[@]} " =~ " ${arg:1} " ]]; then
      log "[ERROR] Invalid short option: ${arg}"
      return 1
    fi
  fi

  if [[ "$arg" =~ ^-- ]]; then
    if [[ ! " ${SUPPORTED_LONG_OPTS[@]} " =~ " ${arg:2} " ]]; then
      log "[ERROR] Invalid long option: ${arg}"
      return 1
    fi
  fi
  return 0
}

check_help_combinations() {
  local args=("$@")
  local has_test=false
  local has_cov=false
  local has_pkg=false

  for arg in "${arg[@]}"; do
    case "$arg" in
      -t|--test) has_test=true ;;
      --cov) has_cov=true ;;
      --pkg) has_pkg=true ;;
      -h|--help) ;;
    esac
  done

  if [[ ("$has_test" == "true") && "$has_pkg" == "true" ]]; then
    log "[ERROR] --pkg cannot be used with test(-t, --test)."
    return 1
  fi
  if [[ "$has_cov" == "true" && ("$has_test" == "true") ]]; then
    log "[ERROR] --cov must be used with test(-t, --test)."
    return 1
  fi
  return 0
}

check_param_with_help() {
  for arg in "$@"; do
    if [[ "$arg" =~ ^- ]]; then
      if ! check_option_validity "$arg"; then
        log "[INFO] Use 'bash build.sh --help' for more information."
        exit 1
      fi
    fi
  done

  seen=()
  for arg in "$@"; do
    arg="${arg%%=*}"
    if [[ " ${seen[@]} " =~ " $arg " ]]; then
      log "[ERROR] $arg can only be input one."
      exit 1
    fi
    seen+=("$arg")
  done

  for arg in "$@"; do
    if [[ "$arg" == "--help" || "$arg" == "-h" ]]; then
      check_help_combinations "$@"
      local comb_result=$?
      if [ $comb_result -eq 1 ]; then
        exit 1
      fi
      SHOW_HELP="general"

      for prev_arg in "$@"; do
        case "$prev_arg" in
          --pkg) SHOW_HELP="package" ;;
          -t|--test) SHOW_HELP="test" ;;
          --make_clean) SHOW_HELP="clean" ;;
        esac
      done

      usage "$SHOW_HELP"
      exit 0
    fi
  done
}

function cmake_config()
{
  local extra_option="$1"
  log "Info: cmake config ${CUSTOM_OPTION} ${extra_option} ."
  cmake ..  ${CUSTOM_OPTION} ${extra_option}
}

function build()
{
  local target="$1"
  cmake --build . --target ${target} -j ${THREAD_NUM}
}

function build_package(){
  cmake_config
  build package
}

function build_test() {
  cmake_config
  build all
}

function set_env()
{
  if [ "${USER_ID}" != "0" ]; then
    DEFAULT_TOOLKIT_INSTALL_DIR="${HOME}/Ascend/ascend-toolkit/latest"
    DEFAULT_INSTALL_DIR="${HOME}/Ascend/latest"
  else
    DEFAULT_TOOLKIT_INSTALL_DIR="/usr/local/Ascend/ascend-toolkit/latest"
    DEFAULT_INSTALL_DIR="/usr/local/Ascend/latest"
  fi

  if [ -n "${cann_path}" ];then
    ASCEND_CANN_PACKAGE_PATH=${cann_path}
  elif [ -n "${ASCEND_HOME_PATH}" ];then
    ASCEND_CANN_PACKAGE_PATH=${ASCEND_HOME_PATH}
  elif [ -n "${ASCEND_OPP_PATH}" ];then
    ASCEND_CANN_PACKAGE_PATH=$(dirname ${ASCEND_OPP_PATH})
  elif [ -d "${DEFAULT_TOOLKIT_INSTALL_DIR}" ];then
    ASCEND_CANN_PACKAGE_PATH=${DEFAULT_TOOLKIT_INSTALL_DIR}
  elif [ -d "${DEFAULT_INSTALL_DIR}" ];then
    ASCEND_CANN_PACKAGE_PATH=${DEFAULT_INSTALL_DIR}
  else
    log "Error: Please set the cann package installation directory through parameter -p|--cann_path."
    exit 1
  fi

  source $ASCEND_CANN_PACKAGE_PATH/bin/setenv.bash || echo "0"
}

check_param_j() {
  if [[ ! $THREAD_NUM =~ ^-?[0-9]+$ ]]; then
   log "[ERROR] -j only support positive integers."
   exit 1
  fi

  if [[ "$THREAD_NUM" -gt "$CPU_NUM" ]]; then
    log "[WARNING] compile thread num:$THREAD_NUM over core num:$CPU_NUM, adjust to core num."
    THREAD_NUM=$CPU_NUM
  fi
}

check_param_clean() {
  if [[ "$#" -gt 1 || ( "$#" -eq 2 && $has_h == "true" ) ]]; then
    log "[ERROR] --make_clean must be used separately."
    exit 1
  fi
}

check_param_test_pkg() {
  if [[ "$TEST" == "all" && "$PKG" == "true" ]]; then
    log "[ERROR] --pkg cannot be used with test(-t, --test)."
    exit 1
  fi
}

check_param_cov() {
  if [[ "$COV" == "true" && "$TEST" != "all" ]]; then
    log "[ERROR] --cov must be used with test(-t, --test)."
    exit 1
  fi
}

set_options() {
  while [[ $# -gt 0 ]]; do
    case $1 in
    -h|--help)
      has_h="true"
      usage
      exit 0
      ;;
    --ccache)
      CCACHE_PROGRAM="$2"
      shift 2
      ;;
    -p=*|--cann_path=*)
      cann_path="${1#*=}"
      shift
      ;;
    -p|--cann_path)
      cann_path="$2"
      shift 2
      ;;
    -t|--test)
      TEST="all"
      check_param_test_pkg
      shift
      ;;
    --asan)
      ASAN="true"
      shift
      ;;
    --cov)
      COV="true"
      check_param_cov
      shift
      ;;
    --pkg)
      PKG="true"
      check_param_test_pkg
      shift
      ;;
    --cann_3rd_lib_path=*)
      CANN_3RD_LIB_PATH="${1#*=}"
      shift
      ;;
    --cann_3rd_lib_path)
      CANN_3RD_LIB_PATH="$2"
      shift 2
      ;;
    --make_clean)
      MAKE_CLEAN="true"
      check_param_clean
      clean
      exit 0
      ;;
    -j=*)
      THREAD_NUM="${1#*=}"
      check_param_j
      shift
      ;;
    -j)
      THREAD_NUM="$2"
      check_param_j
      shift 2
      ;;
    *)
      log "[ERROR] Undefined option: $1"
      usage
      break
      ;;
    esac
  done
}

main() {
  check_param_with_help "$@"
  set_options "$@"
  set_env

  copy_deps_file

  clean

  if [ -n "${TEST}" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_TEST=ON"
  fi

  if [ "${ASAN}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_ASAN=true"
  fi

  if [ "${COV}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DENABLE_GCOV=true"
  fi

  if [ "${PKG}" == "true" ];then
    CUSTOM_OPTION="${CUSTOM_OPTION} -DPACKAGE_OPEN_PROJECT=ON"
  fi

  CUSTOM_OPTION="${CUSTOM_OPTION} -DASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH} -DCANN_3RD_LIB_PATH=${CANN_3RD_LIB_PATH}  -DDEPS_FILE_PATH=${DEPS_FILE_PATH}"

  if [[ ! -d "${BUILD_DIR}" ]]; then
    mkdir -p "${BUILD_DIR}"
  fi
  if [[ ! -d "${OUTPUT_DIR}" ]]; then
    mkdir -p "${OUTPUT_DIR}"
  fi

  cd ${BUILD_DIR}

  if [ -n "${TEST}" ];then
    build_test
  else
    build_package
  fi
}

set -o pipefail

main "$@"
