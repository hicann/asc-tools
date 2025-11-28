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
INSTALL_TYPE_ALL=full # run/devel/docker
INSTALL_TYPE_RUN=run
INSTALL_TYPE_DEV=devel
DEFAULT_USERNAME=${USER}
DEFAULT_USERGROUP=`groups | cut -d" " -f1`
PACKAGE_NAME=asc-tools
LEVEL_INFO="INFO"
LEVEL_WARN="WARNING"
LEVEL_ERROR="ERROR"

INSTALL_INFO_FILE=ascend_install.info
INSTALL_LOG_FILE=ascend_install.log
SHELL_DIR=$(cd "$(dirname "$0")" || exit;pwd)
INSTALL_COMMON_PARSER_PATH="$SHELL_DIR/install_common_parser.sh"
COMMON_SHELL_PATH="$SHELL_DIR/common.sh"
COMMON_INC="$SHELL_DIR/common_func.inc"
FILELIST_PATH="$SHELL_DIR/filelist.csv"
VERSION_INFO="$SHELL_DIR/../../version.info"
LOG_RELATIVE_PATH=var/log/ascend_seclog # install log path and operation log path

log() {
    local content=`echo "$@" | cut -d" " -f2-`
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[Asc-Tookit] [${cur_date}] [$1]: $content" >> "${logFile}"
}

log_and_print() {
    local content=`echo "$@" | cut -d" " -f2-`
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[Asc-Tookit] [${cur_date}] [$1]: $content"
    echo "[Asc-Tookit] [${cur_date}] [$1]: $content" >> "${logFile}"
}

if [ "$(id -u)" -ne 0 ]; then
    home_path=`eval echo "~${USER}"`
    logFile="$home_path/$LOG_RELATIVE_PATH/$INSTALL_LOG_FILE"
else
    logFile="/$LOG_RELATIVE_PATH/$INSTALL_LOG_FILE"
fi

##########################################################################
log $LEVEL_INFO "step into run_asc-tools_install.sh ..."

if [ $# -ne 4 ]; then
    log_and_print $LEVEL_ERROR "input params number error."
    exit 1
fi
install_dir="$2"
install_type="$3"
quiet="$4"

installInfo=$install_dir/$PACKAGE_NAME/$INSTALL_INFO_FILE # install config
if [ ! -f "${installInfo}" ];then
    log_and_print $LEVEL_ERROR "ERR_NO:0x0080;ERR_DES: install info file $installInfo does not exist."
    exit 1
fi

# load shell
source "${COMMON_SHELL_PATH}"

# read user info from install info file
username=$(getInstallParam "UserName" "${installInfo}")
usergroup=$(getInstallParam "UserGroup" "${installInfo}")
checkGroup ${usergroup} ${username}
if [ $? -ne 0 ]; then
    username=$DEFAULT_USERNAME
    usergroup=$DEFAULT_USERGROUP
fi
feature_type=$(getInstallParam "Feature_Type" "${installInfo}")
if [ -z ${feature_type} ]; then
    feature_type="all"
fi

chip_type=$(getInstallParam "Chip_Type" "${installInfo}")

progress_bar(){
    local parent_progress=$1 weight=$2 child_progress=$3
    local output_progress
    output_progress=`awk 'BEGIN{printf "%d\n",('$parent_progress'+'$weight'*'$child_progress'/100)}'`
    log_and_print $LEVEL_INFO "upgradePercentage: ${output_progress}%"
}

copyVersionInfo() {
    # copy version.info to ${install_dir}/asc-tools
    if [ -f "${VERSION_INFO}" ]; then
        cp -f "${VERSION_INFO}" "${install_dir}/${PACKAGE_NAME}"
        if [ $? -ne 0 ]; then
            log_and_print $LEVEL_ERROR "ERR_NO:0x0089;ERR_DES: copy version.info failed."
            return 1
        fi
        changeFileMode 440 "${install_dir}/${PACKAGE_NAME}/version.info"
        chown -hf "${username}:${usergroup}" "${install_dir}/${PACKAGE_NAME}/version.info"
    else
        log_and_print $LEVEL_ERROR "ERR_NO:0x0080;ERR_DES: The file version.info does not exist."
        return 1
    fi
    return 0
}

removeVersionInfo() {
    # remove version.info
    local _version_info="${install_dir}/${PACKAGE_NAME}/version.info"

    if [ -f "${_version_info}" ]; then
        rm -f "${_version_info}"
        if [ $? -ne 0 ]; then
            log_and_print $LEVEL_WARN "ERR_NO:0x0090;ERR_DES: Remove version.info failed."
        fi
    fi
}

installTool()
{
    if [ ! -f "$VERSION_INFO" ]; then
        log_and_print $LEVEL_ERROR "Version info file not exist."
        return 1
    fi
    copyVersionInfo
    [ $? -ne 0 ] && return 1

    local shell_options_="--package=${PACKAGE_NAME} --username=${username} --usergroup=${usergroup} \
--set-cann-uninstall --docker-root=${docker_root_path}"
    . "${COMMON_INC}"
    is_multi_version_pkg "is_multi_version_" "${VERSION_INFO}"
    if [ "${is_multi_version_}" = "true" ]; then
        get_version "version_" "${VERSION_INFO}"
        get_version_dir "version_dir_" "${VERSION_INFO}"
        shell_options_="${shell_options_} --version=${version_} --version-dir=${version_dir_}"
    fi
    if [ "-${setenv}" = "-y" ]; then
        shell_options_="${shell_options_} --setenv"
    fi
    if [ "-${install_for_all}" = "-y" ]; then
        shell_options_="${shell_options_} --install_for_all"
    fi
    local custom_options_="--custom-options=--logfile=$logFile,--quiet=$quiet,--pylocal=$pylocal,\
--feature=$feature_type,--chip=$chip_type"
    if [ -d "$install_dir/tools/ascendc_tools" ];then
        chmod 755 "$install_dir/tools/ascendc_tools"
    fi
    "$INSTALL_COMMON_PARSER_PATH" --install ${shell_options_} ${custom_options_} --feature=$feature_type --chip=$chip_type\
        "${install_type}" "${input_install_path}" "${FILELIST_PATH}"
    if [ -d "$install_dir/tools/ascendc_tools" ];then
        chmod 550 "$install_dir/tools/ascendc_tools"
    fi
    if [ $? -ne 0 ]; then
        log_and_print $LEVEL_ERROR "Install ${PACKAGE_NAME} files failed."
        removeVersionInfo
        return 1
    fi

    chmod -Rf 500 "${install_dir}/${PACKAGE_NAME}/script"
    if [ "$(id -u)" -eq 0 ]; then
        chown -Rf root:root "${install_dir}/${PACKAGE_NAME}/script"
    fi

    checkAllFeature ${feature_type}
}

installProfiling() {
    profiler_install_shell="${install_dir}/${PACKAGE_NAME}/script/install_msprof_fitter.sh"
    if [ ! -f "${profiler_install_shell}" ]; then
        return 0
    fi

    if [ $quiet = y ]; then
        local param_quiet="--quiet"
    fi

    if [ ! x"${install_for_all}" = "x" ] && [ ${install_for_all} = y ]; then
        local param_install_for_all="--install-for-all"
    fi

    "$profiler_install_shell" --install ${param_quiet} ${param_install_for_all}
    if [ $? -ne 0 ];then
        log_and_print $LEVEL_ERROR "Install profiling failed, please check and retry!"
        return 1
    fi

    log $LEVEL_INFO "Install profiling succeed."
    return 0
}

installModule() {
    local shell_info="${install_dir}/${PACKAGE_NAME}/script/shells.info"
    if [ ! -f "${shell_info}" ]; then
        return 0
    fi

    local param_quiet=""
    local param_install_for_all=""
    if [ ! x$quiet = "x" ] && [ $quiet = y ]; then
        param_quiet="--quiet"
    fi
    if [ ! x"${install_for_all}" = "x" ] && [ ${install_for_all} = y ]; then
        param_install_for_all="--install-for-all"
    fi

    shell_array=$(readShellInfo "${shell_info}" "[install]" "[end]")
    for item in ${shell_array[@]}; do
        local shell_path="${install_dir}/${PACKAGE_NAME}/script/${item}"
        if [ ! -f $shell_path ]; then
            log_and_print $LEVEL_WARN "$shell_path not exist."
            continue
        fi
        "${shell_path}" ${param_quiet} ${param_install_for_all}
        if [ $? -ne 0 ];then
            log_and_print $LEVEL_ERROR "Execute $shell_path failed, please check and retry!"
            return 1
        fi
    done
    return 0
}

percent=0
weight=100

installTool
if [ $? -ne 0 ];then
    exit 1
fi

installProfiling
if [ $? -ne 0 ]; then
    exit 1
fi

installModule
if [ $? -ne 0 ]; then
    exit 1
fi

progress_bar $percent $weight 100

exit 0
