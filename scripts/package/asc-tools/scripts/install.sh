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
PACKAGE_NAME=asc-tools
LEVEL_INFO="INFO"
LEVEL_WARN="WARNING"
LEVEL_ERROR="ERROR"

INSTALL_INFO_FILE=ascend_install.info
INSTALL_LOG_FILE=ascend_install.log
DEFAULT_INSTALL_PATH_FOR_ROOT="/usr/local/Ascend"
SHELL_DIR=$(readlink -f $(cd "$(dirname "$0")" || exit; pwd))
INSTALL_SHELL="$SHELL_DIR/run_asc-tools_install.sh"
UNINSTALL_SHELL="$SHELL_DIR/run_asc-tools_uninstall.sh"
COMMON_SHELL="$SHELL_DIR/common.sh"
COMMON_INC="$SHELL_DIR/common_func.inc"
VERSION_INC="$SHELL_DIR/version_cfg.inc"
VERSION_COMPAT_FUNC_PATH="$SHELL_DIR/version_compatiable.inc"
LOG_RELATIVE_PATH=/var/log/ascend_seclog # installing log path and operation log path
VERSION_PATH="$SHELL_DIR/../version.info"
FILELIST_PATH="$SHELL_DIR/filelist.csv"
PKG_INFO_FILE="$SHELL_DIR/../scene.info"
PKG_ARCHITECTURE=$(grep -e "arch" "$PKG_INFO_FILE" | cut --only-delimited -d"=" -f2-) # INSTALL PACKAGES' ARCHITECTURE
PLT_ARCHITECTURE=$(uname -m) # PLATFORM ARCHITECTURE

OPERATION_LOG_FILE=operation.log
LOG_OPERATION_INSTALL="Install"
LOG_OPERATION_UPGRADE="Upgrade"
LOG_OPERATION_UNINSTALL="Uninstall"
LOG_LEVEL_SUGGESTION="SUGGESTION"
LOG_LEVEL_MINOR="MINOR"
LOG_LEVEL_MAJOR="MAJOR"
LOG_LEVEL_UNKNOWN="UNKNOWN"
LOG_RESULT_SUCCESS="success"
LOG_RESULT_FAILED="failed"
OPERATE_ADDR="127.0.0.1"

export pylocal=n
export install_for_all=n
export pre_check=n
export setenv=n
export docker_flag=n
export docker_root_path=""
export input_install_path=""

if [ "-$USER" = "-" ]; then
    # there is no USER in docker
    export USER=$(id -un)
fi

log() {
    local content=`echo "$@" | cut -d" " -f2-`
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[AscTools] [${cur_date}] [$1]: $content" >> "${logFile}"
}

log_and_print() {
    local content=`echo "$@" | cut -d" " -f2-`
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[AscTools] [${cur_date}] [$1]: $content"
    echo "[AscTools] [${cur_date}] [$1]: $content" >> "${logFile}"
}

print_log() {
    local content=`echo "$@" | cut -d" " -f2-`
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[AscTools] [${cur_date}] [$1]: $content"
}

logOperation() {
    local operation=$1
    local errCode=$2
    local cmdList
    local result=$LOG_RESULT_SUCCESS
    local level=$LOG_LEVEL_UNKNOWN

    cmdList=`echo "$@" | cut -d" " -f3-`

    if [ $operation = $LOG_OPERATION_INSTALL ]; then
        level=$LOG_LEVEL_SUGGESTION
    elif [ $operation = $LOG_OPERATION_UPGRADE ]; then
        level=$LOG_LEVEL_MINOR
    elif [ $operation = $LOG_OPERATION_UNINSTALL ]; then
        level=$LOG_LEVEL_MAJOR
    fi

    if [ $errCode -ne 0 ]; then
        result=$LOG_RESULT_FAILED
    fi

    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    content="${operation} ${level} ${USER} ${cur_date} ${OPERATE_ADDR} ${runfilename} ${result}"
    # install type
    if [ $operation = $LOG_OPERATION_INSTALL ];then
        content=$content" install_type="$install_mode
    fi
    content=$content"; cmdlist="$cmdList

    echo $content >> "${optLogFile}"
}

# log file backup
rotateLog() {
    echo "${logFile} {
        su root root
        daily
        size=5M
        rotate 3
        missingok
        create 440 root root
    }" > /etc/logrotate.d/ascend_install
}

# start info before shell executing
startLog() {
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[AscTools] [${cur_date}] [INFO]: Start Time: $cur_date"
    echo "[AscTools] [${cur_date}] [INFO]: Start Time: $cur_date" >> "${logFile}"
}

exitLog() {
    cur_date=`date +"%Y-%m-%d %H:%M:%S"`
    echo "[AscTools] [${cur_date}] [INFO]: End Time: $cur_date"
    echo "[AscTools] [${cur_date}] [INFO]: End Time: $cur_date" >> "${logFile}"
    exit $1
}

checkDirPermission() {
    local path=$1

    if [ x"${path}" = "x" ]; then
        log_and_print $LEVEL_ERROR "dir path ${path} is empty."
        return 1
    fi
    if [ ! -d "${path}" ]; then
        log_and_print $LEVEL_ERROR "dir path does not exist."
        return 1
    fi
    if [ "$(id -u)" -eq 0 ]; then
        return 0
    fi
    if [ ! -r "${path}" ] || [ ! -w "${path}" ] || [ ! -x "${path}" ]; then
        log_and_print $LEVEL_ERROR "The user $USER do not have the permission to access ${path}."
        return 1
    fi
    return 0
}

getUserInfo() {
    username="$(id -nu 2> /dev/null)" # current username
    usergroup=`groups | cut -d" " -f1`
}

checkUserInfo() {
    checkGroup "${usergroup}" "${username}"
    if [ $? -ne 0 ];then
        log_and_print $LEVEL_ERROR "usergroup=${usergroup} not right! Please check the relatianship of ${username} and ${usergroup}"
        exitLog 1
    fi
}

initLog() {
    local _log_path=${LOG_RELATIVE_PATH}
    local _cur_user=${USER}
    local _cur_group=`groups | cut -d" " -f1`

    if [ $(id -u) -ne 0 ]; then
        local _home_path=`eval echo "~"`
        _log_path="${_home_path}${_log_path}"
        if [ ! -d "${_home_path}" ]; then
            print_log $LEVEL_ERROR "ERR_NO:0x0080;ERR_DES: ${_home_path} does not exist."
            exit 1
        fi
    fi

    logFile="${_log_path}/${INSTALL_LOG_FILE}" # install log path
    optLogFile="${_log_path}/${OPERATION_LOG_FILE}" # operate log path

    if [ ! -d "${_log_path}" ]; then
        createFolder "${_log_path}" "${_cur_user}:${_cur_group}" 750
        if [ $? -ne 0 ]; then
            print_log $LEVEL_WARN "create ${_log_path} failed."
        fi
    fi

    if [ ! -f "${logFile}" ]; then
        createFile "${logFile}" "${_cur_user}:${_cur_group}" 640
        if [ $? -ne 0 ]; then
            print_log $LEVEL_WARN "create $logFile failed."
        fi
    fi

    if [ ! -f "${optLogFile}" ]; then
        createFile "${optLogFile}" "${_cur_user}:${_cur_group}" 640
        if [ $? -ne 0 ]; then
            print_log $LEVEL_WARN "create $optLogFile failed."
        fi
    fi
}

# installed version
getVersionInstalled() {
    version2="none"
    if [ -f "$1/version.info" ]; then
        . "$1/version.info"
        version2=${Version}
    fi
    echo $version2
}

# package version
getVersionInRunFile() {
    version1="none"
    if [ -f ./version.info ]; then
        . ./version.info
        version1=${Version}
    fi
    echo $version1
}

logBaseVersion() {
    installed_version=$(getVersionInstalled "${install_dir}/share/info/$PACKAGE_NAME")
    if [ ! "${installed_version}"x = ""x ]; then
        log_and_print $LEVEL_INFO "base version is ${installed_version}."
        return 0
    fi
    log_and_print $LEVEL_WARN "base version was destroyed or not exist."
}

# update install info to asc-tools_install.info
updateInstallInfo() {
    if [ ! -f "${installInfo}" ]; then
        touch "${installInfo}"
        if [ $? -ne 0 ]; then
            exitLog 1
        fi
    fi
    changeFileMode 640 "${installInfo}"

    updateInstallParam "Install_Type" "${install_mode}" "${installInfo}"
    updateInstallParam "Chip_Type" "${chip_type}" "${installInfo}"
    updateInstallParam "Feature_Type" "${feature_type}" "${installInfo}"
    updateInstallParam "UserName" "${username}" "${installInfo}"
    updateInstallParam "UserGroup" "${usergroup}" "${installInfo}"
    updateInstallParam "Install_Path_Param" "${input_install_path}" "${installInfo}"
    updateInstallParam "Docker_Root_Path_Param" "${docker_root_path}" "${installInfo}"

    changeFileMode 440 "${installInfo}"
}

# check target architecture
checkArchitecture() {
    if [ "${PLT_ARCHITECTURE}" != "${PKG_ARCHITECTURE}" ] ; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0001;ERR_DES: The architecture of the run package (${PKG_ARCHITECTURE}) is inconsistent with that of the current environment (${PLT_ARCHITECTURE})."
        exit 1
    fi
}

# check command
checkOperation() {
    if [ $install = y ]; then
        if [ "${run}_${devel}" = "y_y" ] || [ "${run}_${full}" = "y_y" ] || [ "${devel}_${full}" = "y_y" ];then
            log_and_print $LEVEL_ERROR "ERR_NO:0X0004;ERR_DES: Do not specify multiple install mode."
            return 1
        fi
    fi

    if [ "${chip_flag}_${run}_${full}_${devel}_${upgrade}" = "y_n_n_n_n" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Operation failed, '--chip' must be configured together with '--run', '--full', '--upgrade' or '--devel'."
        return 1
    fi

    if [ "${feature_flag}_${run}_${full}_${devel}_${upgrade}" = "y_n_n_n_n" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Operation failed, '--feature' must be configured together with '--run', '--full', '--upgrade' or '--devel'."
        return 1
    fi

    if [ ! $install = y ] && [ ! $upgrade = y ] && [ ! $uninstall = y ] && [ ! $pre_check = y ] && [ ! $check_flag = y ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Operation failed, please specify install type!"
        return 1
    fi

    if [ "${install}_${upgrade}" = "y_y" ] || [ "${install}_${uninstall}" = "y_y" ] || [ "${upgrade}_${uninstall}" = "y_y" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Unsupported parameters, operation failed."
        return 1
    fi

    if [ "${chip_flag}_${uninstall}" = "y_y" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: '--chip' is not supported to used by this way, please use with '--full', '--devel', '--run', '--upgrade'."
        return 1
    fi

    if [ "${feature_flag}_${uninstall}" = "y_y" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: '--feature' is not supported to used by this way, please use with '--full', '--devel', '--run', '--upgrade'."
        return 1
    fi
    return 0
}

check_chmod_length() {
    local mod_num=$1
    local new_mod_num
    mod_num_length=$(expr length "$mod_num")
    if [ $mod_num_length -eq 3 ]; then
        new_mod_num=$mod_num
        echo $new_mod_num
    elif [ $mod_num_length -eq 4 ]; then
        new_mod_num="$(expr substr $mod_num 2 3)"
        echo $new_mod_num
    fi
}

parent_dirs_permision_check(){
    local current_dir="$1" parent_dir="" short_install_dir=""
    local owner="" mod_num=""

    parent_dir=$(dirname "${current_dir}")
    short_install_dir=$(basename "${current_dir}")

    if [ ! -d "${current_dir}" ]; then
        parent_dirs_permision_check "${parent_dir}"
        return $?
    fi

    if [ "${current_dir}"x = "/"x ]; then
        log $LEVEL_INFO "parent_dirs_permision check successfully"
        return 0
    else
        owner=$(stat -c %U "${parent_dir}"/"${short_install_dir}")
        if [ "${owner}" != "root" ]; then
            log_and_print $LEVEL_WARN "The dir [${short_install_dir}] permision not right, it should belong to root."
            return 1
        fi

        mod_num=$(stat -c %a "${parent_dir}"/"${short_install_dir}")
        mod_num=$(check_chmod_length $mod_num)
        if [ -z "${mod_num}" ] || [ "${mod_num}" -lt 755 ]; then
            log_and_print $LEVEL_WARN "The dir [${short_install_dir}] permission is too small, it is recommended that the permission be 755 for the root user."
            return 2
        else
            if [ "${mod_num}" -gt 755 ]; then
                log_and_print $LEVEL_WARN "The dir [${short_install_dir}] permission is too high, it is recommended that the permission be 755 for the root user."
                [ ${quiet} = n ] && return 3
            fi
        fi

        parent_dirs_permision_check "${parent_dir}"
    fi
}

install_path_should_belong_to_root() {
    local ret=0

    if [ "$(id -u)" -ne 0 ]; then
        return 0
    fi

    if [ $uninstall = y ]; then
        return 0
    fi

    parent_dirs_permision_check "${install_dir}" && ret=$? || ret=$?

    # --quiet
    if [ ${quiet} = y ] && [ ${ret} -ne 0 ]; then
        log_and_print $LEVEL_ERROR "the given dir, or its parents, permission is invalid."
        return 1
    fi

    if [ ${ret} -ne 0 ]; then
        print_log $LEVEL_INFO "You are going to put run-files on a unsecure install-path, do you want to continue? [y/n]"
        while true
        do
            read yn
            if [ "$yn" = n ]; then
                return 1
            elif [ "$yn" = y ]; then
                break;
            else
                print_log $LEVEL_ERROR "ERR_NO:0x0002;ERR_DES:input error, please input again!"
            fi
        done
    fi

    return 0
}

getInstallRealPath() {
    local _install_path=$1
    local _prefix=""
    local _home=""
    local _home_path="/root"
    local _install_dir=""

    if [ x"${_install_path}" = "x" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Install path is empty."
        exitLog 1
    fi
    # delete last "/"
    _install_path=`echo "${_install_path}" | sed "s/\/*$//g" | sed "s/\/.$//g"`
    if [ x"${_install_path}" = "x" ]; then
        _install_path="/"
    fi
    # covert relative path to absolute path
    _prefix=`echo "${_install_path}" | cut -d"/" -f1 | cut -d"~" -f1`
    if [ x"${_prefix}" = "x" ]; then
        _install_dir="${_install_path}"
    else
        _prefix=`echo "${run_path}" | cut -d"/" -f1 | cut -d"~" -f1`
        if [ x"${_prefix}" = "x" ]; then
            _install_dir="${run_path}/${_install_path}"
        else
            log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Run package path is invalid: $run_path"
            exitLog 1
        fi
    fi
    # delete last "/."
    _install_dir=`echo "${_install_dir}" | sed "s/\/.$//g"`
    if [ x"${_install_dir}" = "x" ]; then
        _install_dir="/"
    fi
    # covert '~' to home path
    _home=`echo "${_install_dir}" | cut -d"~" -f1`
    if [ "x${_home}" = "x" ]; then
        _install_path=`echo "${_install_dir}" | cut -d"~" -f2`
        if [ "$(id -u)" -ne 0 ]; then
            _home_path=`eval echo "~${USER}"`
            _home_path=`echo "${_home_path}" | sed "s/\/*$//g"`
        fi
        _install_dir="${_home_path}${_install_path}"
    fi
    echo "${_install_dir}"
}

getInstallPath() {
    if [ ! $input_path_flag = y ]; then
        input_install_path=$(getDefaultInstallPath)
    fi
    input_install_path=$(getInstallRealPath ${input_install_path})

    if [ ${docker_flag} = y ]; then
        docker_root_path=$(getInstallRealPath ${docker_root_path})
    fi

    # if docker_flag = n, then docker_root_path is ""
    install_dir="${docker_root_path}${input_install_path}"

    # get multi version dir
    get_version_dir "pkg_version_dir" "${VERSION_PATH}"
    if [ ! -z "$pkg_version_dir" ]; then
        install_dir="${install_dir}/${pkg_version_dir}"
    fi
}

getDefaultInstallPath() {
    local _home_path
    local _install_dir

    if [ "$(id -u)" -eq 0 ]; then
        _install_dir=$DEFAULT_INSTALL_PATH_FOR_ROOT
    else
        _home_path=`eval echo "~${USER}"`
        _home_path=`echo "${_home_path}" | sed "s/\/*$//g"`
        _install_dir="$_home_path/Ascend"
    fi
    echo "${_install_dir}"
}

# get install path
checkInstallPath() {
    check_install_path_valid "${install_dir}"
    if [ $? -ne 0 ]; then
        log_and_print $LEVEL_ERROR "The install_path $install_dir is invalid, only characters in [a-z,A-Z,0-9,-,_] are supported!"
        exitLog 1
    fi
    if [ ! $uninstall = y ]; then
        # check dir is exist
        local _ppath=$(dirname "$install_dir")
        if [ ! -z "$pkg_version_dir" ]; then
            _ppath=$(dirname "$_ppath")
        fi
        if [ -z "${_ppath}" ] || [ ! -d "${_ppath}" ]; then
            log_and_print $LEVEL_ERROR "parent path doesn't exist, please create ${_ppath} first."
            exitLog 1
        fi
    fi
}

# init config file path
initConfPath() {
    installInfo="${install_dir}/share/info/$PACKAGE_NAME/$INSTALL_INFO_FILE" # install info path
}

# create install path and sub directory asc-tools
createInstallPath() {
    if [ ! -z "${pkg_version_dir}" ]; then
        local _ppath=$(dirname "$install_dir")
        if [ ! -d "$_ppath" ]; then
            createFolder "$_ppath" $username:$usergroup 750
            if [ $? -ne 0 ]; then
                log_and_print $LEVEL_ERROR "Create install path($_ppath) failed."
                exitLog 1
            fi
        fi
    fi
    if [ ! -d "${install_dir}" ]; then
        createFolder "${install_dir}" $username:$usergroup 750
        if [ $? -ne 0 ]; then
            log_and_print $LEVEL_ERROR "Create install path failed."
            exitLog 1
        fi
    fi
    if [ ! -d "${install_dir}/share/info/${PACKAGE_NAME}" ]; then
        createFolder "${install_dir}/share/info/${PACKAGE_NAME}" $username:$usergroup 750
        if [ $? -ne 0 ]; then
            log_and_print $LEVEL_ERROR "Create asc-tools path failed in install path."
            exitLog 1
        fi
    fi
}

removeLatestInstallPath() {
    local _install_dir="$1"

    isDirEmpty "${_install_dir:?}/share/info/$PACKAGE_NAME"
    if [ $? -eq 0 ]; then
        # remove asc-tools
        rm -rf "${_install_dir:?}/share/info/$PACKAGE_NAME"
    fi
    isDirEmpty "$_install_dir"
    if [ $? -eq 0 ]; then
        # remove install path
        rm -rf "$_install_dir"
    fi
    if [ ! -z "$pkg_version_dir" ]; then
        # remove install path for muti-version pkg
        local _ppath=$(dirname "$_install_dir")
        isDirEmpty "$_ppath"
        if [ $? -eq 0 ]; then
            rm -rf "$_ppath"
        fi
    fi
}

removeInstallPath() {
    removeLatestInstallPath "$install_dir"
}

removeInstallShell()
{
    local _script_path="$install_dir/share/info/$PACKAGE_NAME/script"
    local _install_shell="$_script_path/install.sh"
    local _pkg_install_shell="$_script_path/run_asc-tools_install.sh"

    chmod +w "$_script_path"
    [ -f "$_install_shell" ] && rm -f "$_install_shell"
    [ -f "$_pkg_install_shell" ] && rm -f "$_pkg_install_shell"
    chmod -w "$_script_path"
}

installRun() {
    if [ -z "$install_mode" -a $upgrade = y ]; then
        install_mode=$INSTALL_TYPE_ALL
    fi
    # create install path
    createInstallPath
    # update install info
    updateInstallInfo
    # start to install
    if [ ! -f "$INSTALL_SHELL" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0X0080;ERR_DES: install shell not exist."
        return 1
    fi
    local _install_shell_path=$(readlink -f "$INSTALL_SHELL")
    "$_install_shell_path" --install "${install_dir}" $install_mode $quiet
    if [ $? -eq 0 ]; then
        log_and_print $LEVEL_INFO "InstallPath: $install_dir"
        if [ ${install} = y ]; then
            log_and_print $LEVEL_INFO "AscTools package installed successfully! The new version takes effect immediately."
        else
            log_and_print $LEVEL_INFO "AscTools package upgraded successfully! The new version takes effect immediately."
        fi
        local _install_path="${input_install_path}"
        if [ ! -z "$pkg_version_dir" ]; then
            _install_path="${_install_path}/${pkg_version_dir}"
        fi
        echo "Please make sure that"
        echo "        - TOOLCHAIN_HOME set with ${_install_path}/share/info/$PACKAGE_NAME"
        removeInstallShell
        return 0
    else
        # roll back
        if [ ! -f "${UNINSTALL_SHELL}" ]; then
            log_and_print $LEVEL_ERROR "ERR_NO:0X0080;ERR_DES: uninstall shell not exist, fallback failed."
        else
            "${UNINSTALL_SHELL}" --uninstall "${install_dir}" $install_mode $quiet

            if [ -f "$installInfo" ]; then
                rm -f "${installInfo}"
            fi
            removeInstallPath
        fi
        if [ ${install} = y ]; then
            log_and_print $LEVEL_ERROR "AscTools package install failed, please retry!"
        else
            log_and_print $LEVEL_ERROR "AscTools package upgrade failed, please retry!"
        fi
        return 1
    fi
}

uninstallRun() {
    checkDirPermission "${install_dir}/share/info/$PACKAGE_NAME"
    if [ $? -ne 0 ]; then
        return 1
    fi

    chattr -i -R "${install_dir}/share/info/$PACKAGE_NAME" > /dev/null 2>&1
    old_uninstall_shell="${install_dir}/share/info/$PACKAGE_NAME/script/run_asc-tools_uninstall.sh"
    if [ ! -f "${old_uninstall_shell}" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0X0080;ERR_DES: uninstall shell not exist."
        return 1
    fi
    local _uninstall_shell_path=$(readlink -f "$old_uninstall_shell")

    install_mode=$(getInstallParam "Install_Type" "${installInfo}")
    if [ -z "$install_mode" ]; then
        log_and_print $LEVEL_WARN "The key Install_Type does not exist in $installInfo, and use default $INSTALL_TYPE_ALL."
        install_mode=$INSTALL_TYPE_ALL
    fi
    "${_uninstall_shell_path}" --uninstall "${install_dir}" "${install_mode}" $quiet
    if [ $? -eq 0 ]; then
        log_and_print $LEVEL_INFO "AscTools package uninstalled successfully! Uninstallation takes effect immediately."

        if [ $uninstall = y ]; then
            rm -f "${installInfo}"
            removeInstallPath
        fi
    else
        log_and_print $LEVEL_ERROR "AscTools package uninstall failed!"
        return 1
    fi
    return 0
}

uninstallLatest() {
    local _install_path=$(readlink -f "${docker_root_path}${input_install_path}")
    local upgrade_version_dir

    get_package_upgrade_version_dir "upgrade_version_dir" "${_install_path}" "asc-tools"
    if [ -z "$upgrade_version_dir" ]; then
        log_and_print $LEVEL_ERROR "AscTools has not bean installed, upgrade failed."
        return 1
    fi

    local _uninstall_shell_path="$_install_path/$upgrade_version_dir/share/info/$PACKAGE_NAME/script/run_asc-tools_uninstall.sh"
    if [ -z "$_uninstall_shell_path" ]; then
        log_and_print $LEVEL_ERROR "ERR_NO:0X0080;ERR_DES: asc-tools uninstall shell not exist."
        return 1
    fi
    local _install_info="$_install_path/$upgrade_version_dir/share/info/$PACKAGE_NAME/$INSTALL_INFO_FILE"
    if [ ! -f "$_install_info" ]; then
        log_and_print $LEVEL_WARN "The install info file not exist, and use default $INSTALL_TYPE_ALL."
        install_mode=$INSTALL_TYPE_ALL
    else
        install_mode=$(getInstallParam "Install_Type" "${_install_info}")
        if [ -z "$install_mode" ]; then
            log_and_print $LEVEL_WARN "The key Install_Type does not exist in $_install_info, and use default $INSTALL_TYPE_ALL."
            install_mode=$INSTALL_TYPE_ALL
        fi
    fi

    "$_uninstall_shell_path" --uninstall "$_install_path/$upgrade_version_dir" "$install_mode" $quiet
    if [ $? -eq 0 ]; then
        log_and_print $LEVEL_INFO "AscTools package uninstalled successfully! Uninstallation takes effect immediately."
        if [ -f "$_install_info" ]; then
            rm -f "$_install_info"
        fi
        removeLatestInstallPath "$_install_path/$upgrade_version_dir"
    else
        log_and_print $LEVEL_ERROR "AscTools package uninstall failed!"
        return 1
    fi
    return 0
}

# uninstall none multi-version package
uninstallOldRun() {
    local _install_path="$docker_root_path/$input_install_path"
    local _install_info="$_install_path/share/info/$PACKAGE_NAME/$INSTALL_INFO_FILE"
    local _version_path="$_install_path/share/info/$PACKAGE_NAME/version.info"

    get_version_dir "_pkg_version_dir" "$_version_path"
    [ ! -z "$_pkg_version_dir" ] && return 0
    [ ! -f "$_install_info" ] && return 0

    local _uninstall_shell_path=$(readlink -f "$_install_path/share/info/$PACKAGE_NAME/script/run_asc-tools_uninstall.sh")
    if [ -z "$_uninstall_shell_path" ]; then
        log_and_print $LEVEL_ERROR "AscTools uninstall shell not exist."
        return 1
    fi
    local _install_type=$(getInstallParam "Install_Type" "${_install_info}")
    if [ -z "$_install_type" ]; then
        log_and_print $LEVEL_WARN "The key Install_Type does not exist in $_install_info, and use default $INSTALL_TYPE_ALL."
        _install_type=$INSTALL_TYPE_ALL
    fi

    "$_uninstall_shell_path" --uninstall "$_install_path" "$_install_type" $quiet
    if [ $? -eq 0 ]; then
        log_and_print $LEVEL_INFO "AscTools old package uninstalled successfully! Uninstallation takes effect immediately."

        if [ -f "$_install_info" ]; then
            rm -f "$_install_info"
        fi
        isDirEmpty "${_install_path:?}/share/info/$PACKAGE_NAME"
        if [ $? -eq 0 ]; then
            rm -rf "${_install_path:?}/share/info/$PACKAGE_NAME"
        fi
        isDirEmpty "$_install_path"
        if [ $? -eq 0 ]; then
            rm -rf "$_install_path"
        fi
    else
        log_and_print $LEVEL_ERROR "AscTools old package uninstall failed!"
        return 1
    fi
    return 0
}

upgradeRun() {
    # uninstall current version package
    if [ -f "$installInfo" ]; then
        uninstallRun
        [ $? -ne 0 ] && return 1
    else
        log_and_print $LEVEL_ERROR "ERR_NO:0x0080;ERR_DES: Package is not installed on the path $install_dir, upgrade failed."
        return 1
    fi
    # install
    installRun
    [ $? -ne 0 ] && return 1
    return 0
}

checkUninstallState() {
    if [ $install = y -a ! $quiet = y ]; then
        version1=$(getVersionInRunFile)
        version2=$(getVersionInstalled $install_dir/share/info/$PACKAGE_NAME)
        print_log $LEVEL_INFO "AscTools package has been installed on the path $install_dir, the version is" \
            "${version2}, and the version of this package is ${version1}," \
            "do you want to continue? [y/n] "
        while true; do
            read yn
            [ "$yn" = n ] && return 1
            [ "$yn" = y ] && break
            print_log $LEVEL_ERROR "ERR_NO:0x0002;ERR_DES:input error, please input again!"
        done
    fi
    return 0
}

startOperation() {
    if [ $uninstall = y ]; then
        if [ -f "$installInfo" ]; then
            uninstallRun
            [ $? -ne 0 ] && return 1
            return 0
        else
            log_and_print $LEVEL_ERROR "ERR_NO:0x0080;ERR_DES: Package is not installed on the path $install_dir, uninstall failed."
            return 1
        fi
    fi

    if [ $install = y ]; then
        if [ -f "$installInfo" ]; then
            checkUninstallState
            [ $? -ne 0 ] && return 0
            uninstallRun
            [ $? -ne 0 ] && return 1
        fi
        installRun
        [ $? -ne 0 ] && return 1
        return 0
    fi

    if [ $upgrade = y ]; then
        upgradeRun
        [ $? -ne 0 ] && return 1
    fi
    return 0
}

preCheck() {
    if [ "-${pre_check}" = "-n" ]; then
        return
    fi

    local check_shell_path="${SHELL_DIR}/../share/info/${PACKAGE_NAME}/bin/prereq_check.bash"
    if [ ! -f ${check_shell_path} ]; then
        log ${LEVEL_WARN} "${check_shell_path} not exist."
        if [ $pre_check_only = y ]; then
            exitLog 0
        fi
        return
    fi
    ${check_shell_path} "${feature_type}"
    local ret=$?
    if [ ${ret} -ne 0 ]; then
        log ${LEVEL_WARN} "execute ${check_shell_path} failed."
    fi

    if [ $pre_check_only = y ]; then
        exitLog ${ret}
    fi
}

checkVersion() {
    if [ ${check_flag} = y ]; then
        if [ -z "$pkg_version_dir" ]; then
            preinstall_check --install-path="${install_dir}" --docker-root="${docker_root_path}" \
                --script-dir="$SHELL_DIR" --package="$PACKAGE_NAME" --logfile="$logFile"
            if [ $? -eq 0 ]; then
                log_and_print ${LEVEL_INFO} "version compatibility check successfully!"
            fi
        fi
        exitLog 0
    elif [ ${install} = y ] || [ ${upgrade} = y ]; then
        if [ -z "$pkg_version_dir" ]; then
            preinstall_process --install-path="${install_dir}" --docker-root="${docker_root_path}" \
                --script-dir="$SHELL_DIR" --package="$PACKAGE_NAME" --logfile="$logFile"
            if [ $? -ne 0 ]; then
                exitLog 1
            else
                log_and_print ${LEVEL_INFO} "version compatibility check successfully!"
            fi
        fi
    fi
}

chip_tmp="all"
chip_result=()
chipTypeCheck(){
    local valid_chip_types=("Ascend310-minirc" "Ascend" "Ascend310" "Ascend310B" "Ascend310P" "Ascend910" "Ascend910B" "Ascend910_93" "Ascend610" "all")
    local Ascend_support_types=("Ascend310B" "Ascend910B" "Ascend910_93" "Ascend610")
    local types_valid=true
    local -a types_array
    IFS=',' read -ra types_array <<< $1

    for chipType in "${types_array[@]}"; do
        if [ -z "chipType" ]; then
            continue
        fi

        if [[ ! " ${valid_chip_types[*]} " =~ " $chipType " ]]; then
            log_and_print $LEVEL_ERROR "Invalid configuration chip type exists:${chipType}."
            exit 1
        fi

        if [ "$chipType" == "Ascend" ]; then
            for ascendSupport in "${Ascend_support_types[@]}"; do
                chip_result+=("$ascendSupport")
            done
        else
            chip_result+=("$chipType")
        fi
    done
    chip_tmp=($(echo "${chip_result[@]}" | tr ' ' '\n' | sort | uniq))
    chip_tmp=($(echo "${chip_tmp[@]}" | tr ' ' ','))
    log $LEVEL_INFO "All chip types are checked successfully [$chip_tmp]."
}

runfilename=$(expr substr $1 5 "$(expr ${#1} - 4)")
run=n
devel=n
full=n
install=n
uninstall=n
upgrade=n
quiet=n
pre_check_only=y
username=""
usergroup=""
input_path_flag=n
feature_type="all"
feature_flag=n
chip_type="all"
chip_flag=n
check_flag=n

# get run file path
run_path=`echo "$2" | cut -d"-" -f3-`
if [ x"${run_path}" = x"" ]; then
    run_path=`pwd`
else
    # delete last "/"
    run_path=`echo "${run_path}" | sed "s/\/*$//g"`
    if [ x"${run_path}" = x"" ]; then
        # root path
        run_path=`pwd`
    fi
fi
shift 2

# load shell
source "${COMMON_SHELL}"
source "${COMMON_INC}"
source "${VERSION_INC}"
source "${VERSION_COMPAT_FUNC_PATH}"

getUserInfo
checkUserInfo
initLog
startLog
log_and_print $LEVEL_INFO "LogFile: $logFile"
log_and_print $LEVEL_INFO "OperationLogFile: $optLogFile"

all_parma="$*"
log_and_print $LEVEL_INFO "InputParams: $all_parma"
if [ "x${all_parma}" = "x" ]; then
    log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Unrecognized parameters. Try './xxx.run --help' for more information."
    exitLog 1
fi

while true
do
    case "$1" in
    --uninstall)
        operation=$LOG_OPERATION_UNINSTALL
        uninstall=y
        pre_check_only=n
        shift
        ;;
    --upgrade)
        operation=$LOG_OPERATION_UPGRADE
        upgrade=y
        pre_check_only=n
        shift
        ;;
    --full)
        install_mode=$INSTALL_TYPE_ALL
        operation=$LOG_OPERATION_INSTALL
        install=y
        full=y
        pre_check_only=n
        shift
        ;;
    --check)
        check_flag=y
        shift
        ;;
    --version)
        log_and_print "package version: $(getVersionInRunFile)"
        exit 0
        ;;
    --run)
        install_mode=$INSTALL_TYPE_RUN
        operation=$LOG_OPERATION_INSTALL
        install=y
        run=y
        pre_check_only=n
        shift
        ;;
    --install-path=*)
        input_path_flag=y
        input_install_path=`echo "$1" | cut -d"=" -f2- `
        shift
        ;;
    --docker-root=*)
        docker_flag=y
        docker_root_path=`echo "$1" | cut -d"=" -f2- `
        shift
        ;;
    --devel)
        install_mode=$INSTALL_TYPE_DEV
        operation=$LOG_OPERATION_INSTALL
        install=y
        devel=y
        pre_check_only=n
        shift
        ;;
    --install-for-all)
        install_for_all=y
        shift
        ;;
    --quiet)
        quiet=y
        shift
        ;;
    --pylocal)
        pylocal=y
        shift
        ;;
    --pre-check)
        pre_check=y
        shift
        ;;
    --setenv)
        setenv=y
        shift
        ;;
    --feature=*)
        feature_type=`echo "$1" | cut -d"=" -f2 `
        [ -z ${feature_type} ] && feature_type="all"
        feature_flag=y
        shift
        ;;
    --chip=*)
        chip_type=`echo "$1" | cut -d"=" -f2 `
        [ -z ${chip_type} ] && chip_type="all"
        chip_flag=y
        shift
        ;;
    *)
        if [ ! "x$1" = "x" ]; then
            log_and_print $LEVEL_ERROR "ERR_NO:0x0004;ERR_DES: Unrecognized parameters: $1. Try './xxx.run --help' for more information."
            exitLog 1
        fi
        break
        ;;
    esac
done

if [ "$feature_type" != "all" ]; then
    contain_feature "ret" "$feature_type" "$FILELIST_PATH"
    if [ "$ret" = "false" ]; then
        log_and_print $LEVEL_WARN "WARNING" "AscTools package doesn't contain features $feature_type, skip installation."
        exit 0
    fi
fi

# check command
checkArchitecture
checkOperation
if [ $? -ne 0 ]; then
    exitLog 1
fi

if [ -z ${chip_type} ]; then
    chip_type="all"
else
    chipTypeCheck $chip_type
    chip_type=$chip_tmp
fi

if [ $(id -u) -eq 0 ]; then
    install_for_all=y
fi

preCheck

getInstallPath
checkInstallPath
checkVersion
install_path_should_belong_to_root
[ $? -ne 0 ] && exitLog 1

initConfPath
logBaseVersion

# start to operate
startOperation
optResult=$?
logOperation $operation $optResult $all_parma
if [ $install = y ] || [ $upgrade = y ]; then
    exitLog $optResult # log operation log
fi
