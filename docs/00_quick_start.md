# 快速入门

## 环境准备<a name="prepare&install"></a>
本项目支持源码编译，在源码编译前，需要确保已经安装驱动、固件和CANN软件（Ascend-cann-toolkit和Ascend-cann-ops）。

  软件安装方式请根据如下描述进行选择：

| 安装方式 | 说明 |使用场景|
| :--- | :--- | :--- |
| 使用WebIDE安装 | WebIDE可提供在线直接运行的昇腾环境，当前可提供单机算力，默认安装最新商发版CANN软件包和固件/驱动包。目前仅适用于Atlas A2系列产品，ARM架构。| 适用于没有昇腾设备的开发者。|
| 使用Docker部署 | Docker镜像是一种CANN高效部署方式，目前仅适用于Atlas A2系列产品，OS仅支持Ubuntu操作系统。|适用有昇腾设备，需要快速搭建环境的开发者。|
| 手动安装软件包 | - |适用有昇腾设备，想体验手动安装CANN包或体验最新master分支能力的开发者。|


### 场景一：使用WebIDE安装

对于无环境的用户，可直接使用WebIDE开发平台，即“**一站式开发平台**”，该平台为您提供在线可直接运行的昇腾环境，环境中已安装必备的软件包，无需手动安装。更多关于开发平台的介绍请参考[LINK](https://gitcode.com/org/cann/discussions/54)。

1. 进入开源项目，单击“`云开发`”按钮，使用已认证过的华为云账号登录。若未注册或认证，请根据页面提示进行注册和认证。

   <img src="./figures/cloudIDE.png" alt="云平台"  width="750px" height="90px">

2. 根据页面提示创建并启动云开发环境，单击“`连接 > WebIDE `”进入算子一站式开发平台，开源项目的资源默认在`/mnt/workspace`目录下。

   <img src="./figures/webIDE.png" alt="云平台"  width="1000px" height="150px">


### 场景二：使用Docker部署

**说明：**
<br>镜像文件比较大，正常网速下，下载时间约为5-10分钟，请您耐心等待。

**1.安装固件和驱动**：请参考[CANN软件安装指南](https://www.hiascend.com/document/redirect/CannCommunityInstWizard)。

**2.下载CANN镜像**

- 步骤1：以root用户登录宿主机。确保宿主机已安装Docker引擎（版本1.11.2及以上）。

- 步骤2：从[昇腾镜像仓库](https://www.hiascend.com/developer/ascendhub/detail/17da20d1c2b6493cb38765adeba85884)拉取已预集成的CANN软件包及所需依赖的镜像。命令如下，根据实际架构选择：

    ```bash
    # 示例：拉取ARM架构的CANN开发镜像
    docker pull --platform=arm64 swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910b-ubuntu22.04-py3.11
    # 示例：拉取X86架构的CANN开发镜像
    docker pull --platform=amd64 swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910b-ubuntu22.04-py3.11
    ```

**3.运行Docker**
<br>拉取镜像后，需要以特定参数启动容器，以便容器内能访问宿主的昇腾设备。

```bash
docker run --name cann_container --device /dev/davinci0 --device /dev/davinci_manager --device /dev/devmm_svm --device /dev/hisi_hdc -v /usr/local/dcmi:/usr/local/dcmi -v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi -v /usr/local/Ascend/driver/lib64/:/usr/local/Ascend/driver/lib64/ -v /usr/local/Ascend/driver/version.info:/usr/local/Ascend/driver/version.info -v /etc/ascend_install.info:/etc/ascend_install.info -it swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910b-ubuntu22.04-py3.11 bash
```
| 参数 | 说明 | 注意事项 |
| :--- | :--- | :--- |
| `--name cann_container` | 为容器指定名称，便于管理。 | 可自定义。 |
| `--device /dev/davinci0` | 核心：将宿主机的NPU设备卡映射到容器内，可指定映射多张NPU设备卡。 | 必须根据实际情况调整：`davinci0`对应系统中的第0张NPU卡。请先在宿主机执行 `npu-smi info`命令，根据输出显示的设备号（如`NPU 0`, `NPU 1`）来修改此编号。|
| `--device /dev/davinci_manager` | 映射NPU设备管理接口。 | - |
| `--device /dev/devmm_svm` | 映射设备内存管理接口。 | - |
| `--device /dev/hisi_hdc` | 映射主机与设备间的通信接口。 | - |
| `-v /usr/local/dcmi:/usr/local/dcmi` | 挂载设备容器管理接口（DCMI）相关工具和库。 | -|
| `-v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi` | 挂载`npu-smi`工具。 | 使容器内可以直接运行此命令来查询NPU状态和性能信息。|
| `-v /usr/local/Ascend/driver/lib64/:/usr/local/Ascend/driver/lib64/` | 关键挂载：将宿主机的NPU驱动库映射到容器内。 | -|
| `-v /usr/local/Ascend/driver/version.info:/usr/local/Ascend/driver/version.info` | 挂载驱动版本信息文件。 | -|
| `-v /etc/ascend_install.info:/etc/ascend_install.info` | 挂载CANN软件安装信息文件。 |- |
| `-v /home/your_dir:/home/your_dir` | 挂载宿主机的一个路径到容器中。 | 可自选使用。 |
| `-it` | `-i`（交互式）和 `-t`（分配伪终端）的组合参数。 |- |
| `swr.cn-south-1.myhuaweicloud.com/ascendhub/cann:9.0.0-beta.1-910b-ubuntu22.04-py3.11` | 指定要运行的Docker镜像。 |请确保此镜像名和标签（tag）与你通过`docker pull`拉取的镜像完全一致。 |
| `bash` | 容器启动后立即执行的命令。 |- |

### 场景三：手动安装软件包

**场景1：体验master版本能力或基于master版本进行开发**

如果您想体验**master分支最新能力**，单击[下载链接](https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-mirror/software/master)获取软件包，按照如下步骤进行安装。更多安装指导请参考[CANN软件安装指南](https://www.hiascend.com/document/redirect/CannCommunityInstWizard)。

1. **安装驱动与固件（运行态依赖）**

   运行算子时必须安装驱动与固件，若仅编译算子，可跳过本操作，安装指导详见《[CANN 软件安装指南](https://www.hiascend.com/document/redirect/CannCommunityInstSoftware)》。

2. **安装社区尝鲜版CANN toolkit包**

    根据实际环境，下载对应`Ascend-cann-toolkit_${cann_version}_linux-${arch}.run`包，下载链接为[toolkit x86_64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/x86_64/Ascend-cann-toolkit_9.0.0_linux-x86_64.run)、[toolkit aarch64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/aarch64/Ascend-cann-toolkit_9.0.0_linux-aarch64.run)。

    ```bash
    # 确保安装包具有可执行权限
    chmod +x Ascend-cann-toolkit_${cann_version}_linux-${arch}.run
    # 安装命令
    ./Ascend-cann-toolkit_${cann_version}_linux-${arch}.run --full --force --install-path=${install_path}
    ```
    - \$\{cann\_version\}：表示CANN包版本号。
    - \$\{arch\}：表示CPU架构，如aarch64、x86_64。
    - \$\{install\_path\}：表示指定安装路径。
    - 缺省--install-path时， 则使用默认路径安装。
    若使用root用户安装，安装完成后相关软件存储在“/usr/local/Ascend”路径下；若使用非root用户安装，安装完成后相关软件存储在“$HOME/Ascend”路径下。

3. **安装社区版CANN ops包（运行态依赖）**

    运行算子前必须安装本包，若仅编译算子，可跳过本操作。

    根据产品型号和环境架构，下载对应`Ascend-cann-${soc_name}-ops_9.0.0_linux-${arch}.run`包，下载链接如下：

    - Atlas A2 训练系列产品/Atlas A2 推理系列产品：[ops x86_64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/x86_64/Ascend-cann-910b-ops_9.0.0_linux-x86_64.run)、[ops aarch64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/aarch64/Ascend-cann-910b-ops_9.0.0_linux-aarch64.run)。
    - Atlas A3 训练系列产品/Atlas A3 推理系列产品：[ops x86_64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/x86_64/Ascend-cann-A3-ops_9.0.0_linux-x86_64.run)、[ops aarch64包](https://mirror-centralrepo.devcloud.cn-north-4.huaweicloud.com/artifactory/cann-run-release/software/master/20260211182015/aarch64/Ascend-cann-A3-ops_9.0.0_linux-aarch64.run)。
    - Ascend 950PR/Ascend 950DT：[ops x86_64包](https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-release/software/master/20260213000325157/x86_64/Ascend-cann-950-ops_9.0.0_linux-x86_64.run)、[ops aarch64包](https://ascend.devcloud.huaweicloud.com/artifactory/cann-run-release/software/master/20260213000325157/aarch64/Ascend-cann-950-ops_9.0.0_linux-aarch64.run)。

    ```bash
    # 确保安装包具有可执行权限
    chmod +x Ascend-cann-${soc_name}-ops_9.0.0_linux-${arch}.run
    # 安装命令
    ./Ascend-cann-${soc_name}-ops_9.0.0_linux-${arch}.run --install --install-path=${install_path}
    ```
    - \$\{soc\_name\}：表示NPU型号名称。
    - \$\{arch\}：表示CPU架构，如aarch64、x86_64。
    - \$\{install\_path\}：表示指定安装路径，需要与toolkit包安装在相同路径，root用户默认安装在`/usr/local/Ascend`目录，非root用户默认安装在`$HOME/Ascend`目录。

**场景2：体验已发布版本能力或基于已发布版本进行开发**

如果您想体验**官网正式发布的CANN包**能力，请访问[CANN官网下载中心](https://www.hiascend.com/cann/download)，选择对应版本CANN软件包（仅支持CANN 8.5.0及后续版本）进行安装。

## 配置环境变量

### WebIDE场景

  ```bash
  source /home/developer/Ascend/cann/set_env.sh
  ```

### 其他场景

- 默认路径，root用户安装

    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

- 默认路径，非root用户安装
    ```bash
    source $HOME/Ascend/cann/set_env.sh
    ```

- 指定路径安装
    ```bash
    source ${install_path}/cann/set_env.sh
    ```

## 下载源码

    可以使用以下两种方式下载，请选择其中一种进行源码准备。

- 命令行方式下载（下载时间较长，但步骤简单）。

  ```bash
  # 开发环境，非root用户命令行中执行以下命令下载源码仓。git_clone_path为用户自己创建的某个目录。
  cd ${git_clone_path}
  git clone https://gitcode.com/cann/asc-tools.git
  ```
  **注：如果需要切换到其它tag版本，以v0.5.0为例，可执行以下命令。**
  ```bash
  git checkout v0.5.0
  ```
- 若您的编译环境无法访问网络，可以通过压缩包方式下载（下载时间较短，但步骤稍微复杂）。

  **注：如果需要下载其它版本代码，请先请根据前置条件说明进行asc-tools仓分支切换。下载压缩包命名跟tag/branch相关，此处以master分支为例，下载的名字将会是asc-tools-master.zip**
  ```bash
  # 1. asc-tools仓右上角选择 【下载ZIP】。
  # 2. 将ZIP包上传到开发环境中的普通用户某个目录中，【例如：${git_clone_path}/asc-tools-master.zip】。
  # 3. 开发环境中，执行以下命令，解压zip包。
  cd ${git_clone_path}
  unzip asc-tools-master.zip
  ```
  并且，需要同样操作下载依赖的[msot](https://gitcode.com/Ascend/msot.git)、[mssanitizer](https://gitcode.com/Ascend/mssanitizer.git)、[msopprof](https://gitcode.com/Ascend/msopprof.git)、[msopgen](https://gitcode.com/Ascend/msopgen.git)、[mskpp](https://gitcode.com/Ascend/mskpp.git)、[mskl](https://gitcode.com/Ascend/mskl.git)、[msdebug](https://gitcode.com/Ascend/msdebug.git)代码仓的master分支的压缩包，即点击各仓链接，右上角选择【下载ZIP】。

  同时，需要根据实际环境，下载对应Release版本的闭源cpudebug包（[cpudebug x86_64包](https://container-obsfs-filesystem.obs.cn-north-4.myhuaweicloud.com/package/cann/asc-toolkit-dev/version_compile/master/202601/20260131_174635_/ubuntu_x86/cann-asc-tools-cpudebug-deps-lib_release_9.0.0_linux-x86_64.tar.gz)、[cpudebug aarch64包](https://container-obsfs-filesystem.obs.cn-north-4.myhuaweicloud.com/package/cann/asc-toolkit-dev/version_compile/master/202601/20260131_174635_/ubuntu_aarch64/cann-asc-tools-cpudebug-deps-lib_release_9.0.0_linux-aarch64.tar.gz)），以及开源第三方软件依赖，列表如下：

  | 开源软件 | 版本 | 下载地址 |
  |---|---|---|
  | makeself | 2.5.0 | [makeself-2.5.0.tar.gz](https://gitcode.com/cann-src-third-party/makeself/releases/download/release-2.5.0-patch1.0/makeself-release-2.5.0-patch1.tar.gz) |
  | boost | 1.87.0 | [boost-1_87_0.tar.gz](https://gitcode.com/cann-src-third-party/boost/releases/download/v1.87.0/boost_1_87_0.tar.gz) |
  | googletest | 1.14.0 | [googletest-1.14.0.tar.gz](https://gitcode.com/cann-src-third-party/googletest/releases/download/v1.14.0/googletest-1.14.0.tar.gz) |
  | mockcpp | 2.7 | [mockcpp-2.7.tar.gz](https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h2/mockcpp-2.7.tar.gz) |
  | mockcpp_patch | 2.7 | [mockcpp-2.7.patch](https://gitcode.com/cann-src-third-party/mockcpp/releases/download/v2.7-h3/mockcpp-2.7_py3-h3.patch) |

  其中，对于依赖的开源第三方软件，提供一键式下载脚本`install_dep_tar.py`，用户可以通过执行如下命令，下载所有开源第三方软件依赖。
  ```bash
  python3 install_dep_tar.py --dest_dir=${your_3rd_party_path} # 其中，${your_3rd_party_path}为下载开源第三方软件的存放路径
  ```

## 安装依赖

   以下所列仅为本开源仓源码编译用到的依赖，其中python、gcc、cmake的安装方法请参见配套版本的[用户手册](https://hiascend.com/document/redirect/CannCommunityInstDepend)，选择安装场景后，参见“安装CANN > 安装依赖”章节进行相关依赖的安装。

   - python >= 3.7.0

   - gcc >= 7.3.0

   - cmake >= 3.16.0

   - ccache >= 4.6.1

     建议版本[release-v4.6.1](https://github.com/ccache/ccache/releases/tag/v4.6.1)，x86_64环境[下载链接](https://github.com/ccache/ccache/releases/download/v4.6.1/ccache-4.6.1-linux-x86_64.tar.xz)，aarch64环境[下载链接](https://github.com/ccache/ccache/releases/download/v4.6.1/ccache-4.6.1.tar.gz)。

     x86_64环境安装步骤如下：
     
     ```bash
     # 在准备安装的路径下创建buildtools目录，如有则忽略
     # 这里以安装路径/opt为例，对安装命令进行说明
     mkdir /opt/buildtools
     # 切换到安装包下载路径，将ccache解压到安装路径
     tar -xf ccache-4.6.1-linux-x86_64.tar.xz -C /opt/buildtools
     chmod 755 /opt/buildtools/ccache-4.6.1-linux-x86_64/ccache
     mkdir -p /usr/local/ccache/bin
     # 建立软链接
     ln -sf /opt/buildtools/ccache-4.6.1-linux-x86_64/ccache /usr/local/bin/ccache
     ln -sf /opt/buildtools/ccache-4.6.1-linux-x86_64/ccache /usr/local/ccache/bin/ccache
     # 将ccache添加到环境变量PATH
     export PATH=/usr/local/ccache/bin:$PATH
     ```
     
     aarch64环境安装步骤如下：
     - 下载依赖项  
       下载[zstd](https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz)和[hiredis](https://github.com/redis/hiredis/archive/refs/tags/v1.0.2.tar.gz)。
    
     - 编译安装
     
        ```bash
        # 在准备安装的路径下创建buildtools目录，如有则忽略
        # 这里以安装路径/opt为例，对安装命令进行说明
        mkdir /opt/buildtools
        # 切换到安装包下载路径，将zstd解压到安装路径
        tar -xf zstd-1.5.0.tar.gz -C /opt/buildtools
        cd /opt/buildtools/zstd-1.5.0
        make -j 24
        make install
        cd -
        # 切换到安装包下载路径，将hiredis解压到安装路径
        tar -xf hiredis-1.0.2.tar.gz -C /opt/buildtools
        cd /opt/buildtools/hiredis-1.0.2
        make -j 24 prefix=/opt/buildtools/hiredis-1.0.2 all
        make prefix=/opt/buildtools/hiredis-1.0.2 install
        cd -
        # 切换到安装包下载路径，将ccache解压到安装路径
        tar -xf ccache-4.6.1.tar.gz -C /opt/buildtools
        cd /opt/buildtools/ccache-4.6.1
        mkdir build
        cd build/
        cmake -DCMAKE_BUILD_TYPE=Release -DZSTD_LIBRARY=/usr/local/lib/libzstd.a -DZSTD_INCLUDE_DIR=/usr/local/include -DHIREDIS_LIBRARY=/usr/local/lib/libhiredis.a -DHIREDIS_INCLUDE_DIR=/usr/local/include ..
        make -j 24
        make install
        mkdir -p /usr/local/ccache/bin
        # 建立软链接
        ln -sf /usr/local/bin/ccache /usr/local/ccache/bin/ccache
        # 将ccache添加到环境变量PATH
        export PATH=/usr/local/ccache/bin:$PATH
        ```

   - lcov >= 1.13（可选，仅执行UT时依赖）
   
     以Ubuntu系统为例，x86_64环境执行以下命令安装：
     ```bash
     apt install lcov
     ```
     以Euler系统为例，aarch64环境执行以下命令安装：
     ```bash
     yum install lcov
     ```

   - pytest >= 8.3.2（可选，仅执行UT时依赖）

     执行以下命令安装：
     ```bash
     pip3 install pytest
     ```
   
   - coverage >= 4.5.4（可选，仅执行UT时依赖）

     执行以下命令安装：
     ```bash
     pip3 install coverage
     ```
   - googletest（可选，仅执行UT时依赖，建议版本[release-1.11.0](https://github.com/google/googletest/releases/tag/release-1.11.0)）

     下载[googletest源码](https://github.com/google/googletest.git)后，执行以下命令安装：

     ```bash
     mkdir temp && cd temp                 # 在googletest源码根目录下创建临时目录并进入
     cmake .. -DCMAKE_CXX_FLAGS="-fPIC -D_GLIBCXX_USE_CXX11_ABI=0"
     make
     make install                         # root用户安装googletest
     # sudo make install                  # 非root用户安装googletest
     ```

## 编译安装<a name="compile&install"></a>

1. 编译

  本开源仓提供一键式编译安装能力，进入本开源仓代码根目录，执行如下命令：

  ```bash
  cd asc-tools
  bash build.sh --pkg
  ```
  若您的编译环境无法访问网络，您需要在联网环境中下载上述依赖代码仓的压缩包、闭源压缩包及开源软件压缩包，并手动上传至您的编译环境中。

  您需要在编译环境中新建一个`{your_3rd_party_path}`目录来存放依赖代码仓、闭源及第三方开源软件的压缩包。

  ```bash
  mkdir -p {your_3rd_party_path}
  ```

  创建好目录后，将下载好的所依赖的压缩包上传至目录`{your_3rd_party_path}`后，可以使用如下命令进行编译：
  ```bash
  bash build.sh --pkg --cann_3rd_lib_path={your_3rd_party_path}
  ```

  编译完成后会在`build_out`目录下生成cann-asc-tools_*<cann_version>*_linux-*<arch>*.run软件包。

2. 安装

  在开源仓根目录下执行下列命令，根据设置的环境变量路径，将编译生成的run包安装到CANN包的装包路径，同时会覆盖原CANN包中的Ascend C内容。

  ```bash
  # 切换到run包生成路径下
  cd build_out
  # 默认路径安装run包
  ./cann-asc-tools_<cann_version>_linux-<arch>.run --full
  # 指定路径安装run包
  ./cann-asc-tools_<cann_version>_linux-<arch>.run --full --install-path=${install_path}
  ```

## UT测试（可选）

在开源仓根目录执行下列命令之一，将依次批跑tests目录下的用例，得到结果日志，用于看护编译是否正常。

```bash
bash build.sh -t
```

或

```bash
bash build.sh --test
```

若您的环境无法访问网络，您需要在联网环境中下载上述依赖代码仓的压缩包及开源软件压缩包，并手动上传至您的环境中。
  同时，相比编译，UT不下载Release版本的cpudebug包，而是需要根据实际环境，下载对应Debug版本的闭源cpudebug包（[cpudebug x86_64包](https://container-obsfs-filesystem.obs.cn-north-4.myhuaweicloud.com/package/cann/asc-toolkit-dev/version_compile/master/202601/20260131_174635_/ubuntu_x86/cann-asc-tools-cpudebug-deps-lib_debug_9.0.0_linux-x86_64.tar.gz)、[cpudebug aarch64包](https://container-obsfs-filesystem.obs.cn-north-4.myhuaweicloud.com/package/cann/asc-toolkit-dev/version_compile/master/202601/20260131_174635_/ubuntu_aarch64/cann-asc-tools-cpudebug-deps-lib_debug_9.0.0_linux-aarch64.tar.gz)），Debug版本相比Release版本，方便进行调试。

  您需要在环境中新建一个`{your_3rd_party_path}`目录来存放依赖代码仓、闭源及第三方开源软件的压缩包。

  ```bash
  mkdir -p {your_3rd_party_path}
  ```

  创建好目录后，将下载好的所依赖的压缩包上传至目录`{your_3rd_party_path}`后，可以使用如下命令进行UT测试：
  ```bash
  bash build.sh --test --cann_3rd_lib_path={your_3rd_party_path}
  ```

### UT测试显示覆盖率

- 依赖项
    - lcov >= 1.14

- 执行命令

```bash
bash build.sh --test --cov
```