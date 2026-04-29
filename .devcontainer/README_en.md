# Dev Container

[![Docker Version](https://img.shields.io/badge/docker-%3E%3D23.0.0-blue.svg)](https://docs.docker.com/)
[![Docker Buildx](https://img.shields.io/badge/docker%20buildx-required-orange.svg)](https://docs.docker.com/reference/cli/docker/buildx/)

[中文版](./README.md) | [English](./README_en.md)

Ubuntu 24.04 containerized development environment for AscendC NPU kernel development.

## 📋 Prerequisites

| Requirement | With NPU | Without NPU |
| ------------- | :----------: | :-------------: |
| Docker ≥ 23.0.0 | required | required |
| docker buildx | required | required |
| Ascend driver on host (`/usr/local/Ascend/driver`) | required | not needed |
| NPU device nodes (`/dev/davinciN`, etc.) | required | not needed |

> [!IMPORTANT]
> The host's Ascend driver directory (`/usr/local/Ascend/driver`) is mounted into the container as **read-only**. The **CANN toolkit and ops packages** should be installed inside the container after startup, not on the host — installing on the host tends to pollute the host environment and cause confusing environment variable exports.

### Check Docker and buildx

Confirm Docker is installed with version ≥ 23.0.0:

```bash
docker --version
```

If the command is missing or the version is too low, follow the [Docker official installation guide](https://docs.docker.com/engine/install/) to install or upgrade.

> [!TIP]
> It is recommended to add the current user to the `docker` group to avoid needing `sudo` every time:
> ```bash
> sudo usermod -aG docker $USER && newgrp docker
> ```

Image building depends on the `docker buildx` plugin. Check:

```bash
docker buildx version
```

if the command is not found, install it:

```bash
mkdir -p /usr/local/lib/docker/cli-plugins
ARCH=$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')
curl -fsSL "https://github.com/docker/buildx/releases/latest/download/buildx-linux-${ARCH}" \
     -o /usr/local/lib/docker/cli-plugins/docker-buildx
chmod +x /usr/local/lib/docker/cli-plugins/docker-buildx
```

## 🧑‍💻 Quick Start (Human Users)

> [!NOTE]
> If you are an AI Agent, skip to [Quick Start (AI Agent)](#quick-start-ai-agent).
>
> The following steps assume you are developing with VS Code on a **Linux host** (the Ascend driver only supports Linux).

### Step 1: Install VS Code

Download and install the version for your platform from the [Visual Studio Code](https://code.visualstudio.com/) website.

### Step 2: Install the Dev Containers extension

In VS Code, press `Ctrl+Shift+X` to open the Extensions view, then search for and install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension.

### Step 3: Clone the repository

```bash
git clone https://gitcode.com/cann/asc-tools.git
cd asc-tools
```

### Step 4: Prepare based on NPU availability

#### 4.1 With NPU: verify driver and device nodes

On the host, run:

```bash
npu-smi info                        # check NPU status
ls /dev/davinci*                    # confirm device nodes
ls /usr/local/Ascend/driver         # confirm driver directory
```

If any command fails or returns no output, the Ascend driver is not ready. Refer to the [CANN Software Installation Guide - Install on Physical Machine](https://www.hiascend.com/document/redirect/CannCommunityInstWizard).

If the host has fewer than 8 NPUs, edit `.devcontainer/devcontainer.json` and remove the extra `--device=/dev/davinciN` lines (run `ls /dev/davinci*` to check the actual device count).

#### 4.2 Without NPU: adjust devcontainer.json

Machines without NPU must first remove device and driver mount configurations.

**In `runArgs`**, remove all NPU-related entries: the device access flags (`--ipc=host`, `--net=host`, `--privileged`) and `--device` lines (`davinci0-7`, `davinci_manager`, `devmm_svm`, `hisi_hdc`), and set it to:

```jsonc
"runArgs": []
```

**In `mounts`**, remove the driver and device-related bind mounts:

```text
source=/usr/local/Ascend/driver,...
source=/usr/local/dcmi,...
source=/usr/local/bin/npu-smi,...
source=/etc/ascend_install.info,...
```

Keep the `ccache` volume and any data directories you added yourself.

### Step 5: Open the project in the container

Open this project directory in VS Code, press `Ctrl+Shift+P`, and run:

```text
Dev Containers: Reopen in Container
```

The Dev Containers extension will automatically build the image and start the container. The first build takes about 5 minutes (mostly downloading the conda environment and PyTorch); subsequent opens hit the Docker layer cache and complete in seconds.

### Step 6: Install CANN inside the container

After the container starts, in the container terminal refer to the [📥 Download and Install CANN Packages](../docs/quick_start.md#cann-install) section to download and install the toolkit and ops packages.

### Step 7: Verify environment

In the container terminal, refer to the [✅ Environment Verification](../docs/quick_start.md#cann-verify) section to confirm NPU device and CANN package status, then follow [⚙️ Environment Variable Configuration](../docs/quick_start.md#cann-env-setup) to load the environment variables. You can then begin kernel development.

## 🤖 Quick Start (AI Agent)<a name="quick-start-ai-agent"></a>

AI Agents (such as Claude Code, CI runners) can run the container directly without VS Code.

**Before running the container, confirm the following with the user:**

1. **Repository path** — where is the project checked out on the host? (default: current directory `$PWD`).
2. **Data directories** — are there other data directories to mount (e.g. datasets, model weights)?
3. **NPU availability** — does the host have Ascend NPUs? (determines which variant to use below).

After confirmation, **show the user the exact `docker run` command** you are about to execute and wait for user confirmation before proceeding.

For Docker and buildx checks and installation, see [Check Docker and buildx](#check-docker-and-buildx).

### Build the image

```bash
docker buildx build --network host -t ascendc:ubuntu24.04 .devcontainer/
```

> [!TIP]
> The first build takes about 5 minutes (mostly downloading the conda environment and PyTorch). For subsequent rebuilds, if only non-package layers are changed, the Docker layer cache will be hit and the build completes in seconds.

### With NPU

```bash
docker run -itd --name ascendc_container \
  --ipc=host --net=host --privileged \
  --device=/dev/davinci0 \
  --device=/dev/davinci1 \
  --device=/dev/davinci2 \
  --device=/dev/davinci3 \
  --device=/dev/davinci4 \
  --device=/dev/davinci5 \
  --device=/dev/davinci6 \
  --device=/dev/davinci7 \
  --device=/dev/davinci_manager \
  --device=/dev/devmm_svm \
  --device=/dev/hisi_hdc \
  -v /usr/local/dcmi:/usr/local/dcmi:ro \
  -v /usr/local/bin/npu-smi:/usr/local/bin/npu-smi:ro \
  -v /usr/local/Ascend/driver:/usr/local/Ascend/driver:ro \
  -v /etc/ascend_install.info:/etc/ascend_install.info:ro \
  ascendc:ubuntu24.04
```

If the host has fewer than 8 NPUs, remove the corresponding `--device=/dev/davinciN` lines (run `ls /dev/davinci*` to check the actual device count).

### Without NPU

```bash
docker run -itd --name ascendc_container ascendc:ubuntu24.04
```

> [!IMPORTANT]
>
> - If the user needs to mount data directories, append each as `-v /host/path:/container/path`.
> - After the container starts, CANN toolkit and ops packages must be installed manually inside the container. Ask the user for the CANN package paths or installer commands, or refer to the [📥 Download and Install CANN Packages](../docs/quick_start.md#cann-install) section.

## 🐍 Python Environment

The container comes with the following conda environment pre-installed:

| Environment | Python | PyTorch |
| :--------: | :--------: | :---------: |
| `py312` | 3.12 | 2.7.1 |

Default active environment is `py312`.

## ⚙️ Configuration

### Mirror Sources

Default mirror sources can be overridden at build time via build arguments:

```bash
docker buildx build --network host \
  --build-arg APT_MIRROR=mirrors.ustc.edu.cn \
  --build-arg CONDA_MIRROR=https://mirrors.ustc.edu.cn/anaconda \
  --build-arg PYPI_MIRROR=https://pypi.mirrors.ustc.edu.cn/simple \
  -t ascendc:ubuntu24.04 .devcontainer/
```

Available mirrors:

| Build arg | Default | Tsinghua | USTC | Huawei |
| :----------: | ------ | ---------- | -------- | -------- |
| `APT_MIRROR` | `mirrors.huaweicloud.com` | `mirrors.tuna.tsinghua.edu.cn` | `mirrors.ustc.edu.cn` | `mirrors.huaweicloud.com` |
| `CONDA_MIRROR` | `mirrors.tuna.tsinghua.edu.cn/anaconda` | `mirrors.tuna.tsinghua.edu.cn/anaconda` | `mirrors.ustc.edu.cn/anaconda` | — |
| `PYPI_MIRROR` | `repo.huaweicloud.com/repository/pypi/simple` | `pypi.tuna.tsinghua.edu.cn/simple` | `pypi.mirrors.ustc.edu.cn/simple` | `repo.huaweicloud.com/repository/pypi/simple` |

### Mount Directories

Mount directories vary by developer; add your own in `devcontainer.json`:

```jsonc
// mounts
"source=/your/data,target=/your/data,type=bind"
```

### NPU Device Selection

Device nodes are passed explicitly with `--device=/dev/davinciN`. If the host has fewer than 8 NPUs, remove the corresponding lines. For example, for a 4-NPU host:

```bash
--device=/dev/davinci0 \
--device=/dev/davinci1 \
--device=/dev/davinci2 \
--device=/dev/davinci3 \
```
