# Build:
#   docker buildx build --network host -t ascendc:ubuntu24.04 .devcontainer/
#
# Mirror sources (override with --build-arg):
#   APT_MIRROR:   mirrors.huaweicloud.com (default) | mirrors.tuna.tsinghua.edu.cn | mirrors.ustc.edu.cn
#   CONDA_MIRROR: mirrors.tuna.tsinghua.edu.cn/anaconda (default) | mirrors.ustc.edu.cn/anaconda
#   PYPI_MIRROR:  repo.huaweicloud.com/repository/pypi/simple (default) | pypi.tuna.tsinghua.edu.cn/simple

# Stage 0: extract conda (multi-arch, matches host architecture automatically)
FROM continuumio/miniconda3 AS conda-base

# Stage 1: main image
FROM ubuntu:24.04

# ============================================================
# Build arguments — mirror sources
# ============================================================
ARG APT_MIRROR=mirrors.huaweicloud.com
ARG CONDA_MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda
ARG PYPI_MIRROR=https://repo.huaweicloud.com/repository/pypi/simple
ARG REPO_MIRROR=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo

# ============================================================
# Base environment
# ============================================================
ENV TZ=Asia/Shanghai \
    LANG=C.UTF-8 \
    SHELL=/bin/zsh \
    DEBIAN_FRONTEND=noninteractive \
    PATH=/opt/conda/bin:$PATH

USER root
WORKDIR /tmp

# ============================================================
# APT mirror & system packages (single layer to avoid stale cache)
# ============================================================
RUN arch="$(dpkg --print-architecture)" && \
    if [ "${arch}" = "arm64" ]; then \
      sed -i "s@//ports.ubuntu.com@//${APT_MIRROR}@g" /etc/apt/sources.list.d/ubuntu.sources; \
    else \
      sed -i "s@//archive.ubuntu.com@//${APT_MIRROR}@g; s@//security.ubuntu.com@//${APT_MIRROR}@g" \
        /etc/apt/sources.list.d/ubuntu.sources; \
    fi && \
    apt update && \
    apt install -y --no-install-recommends \
      tzdata lsb-release curl wget gnupg2 openssl ca-certificates openssh-client pciutils \
      build-essential cmake ninja-build ccache pkg-config autoconf automake libtool gperf \
      gdb gdbserver git git-lfs lcov vim htop tree zsh clangd \
    && ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone \
    && rm -rf /var/lib/apt/lists/*

# ============================================================
# repo tool
# ============================================================
RUN curl -fsSL ${REPO_MIRROR} -o /usr/local/bin/repo && \
    chmod a+x /usr/local/bin/repo

# ============================================================
# git global config
# ============================================================
RUN git config --global core.autocrlf input && \
    git config --global core.editor vim && \
    git config --global color.ui auto && \
    git config --global alias.lg "log --color --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit"

# ============================================================
# Trust gitcode.com SSH host key
# ============================================================
RUN mkdir -p /root/.ssh && \
    ssh-keyscan -H gitcode.com >> /root/.ssh/known_hosts && \
    chmod 600 /root/.ssh/known_hosts

# ============================================================
# oh-my-zsh + plugins (via gitcode mirror)
# ============================================================
RUN git clone --depth=1 https://gitcode.com/ohmyzsh/ohmyzsh.git /root/.oh-my-zsh && \
    cp /root/.oh-my-zsh/templates/zshrc.zsh-template /root/.zshrc && \
    git clone --depth=1 https://gitcode.com/zsh-users/zsh-autosuggestions \
        /root/.oh-my-zsh/custom/plugins/zsh-autosuggestions && \
    git clone --depth=1 https://gitcode.com/zsh-users/zsh-syntax-highlighting \
        /root/.oh-my-zsh/custom/plugins/zsh-syntax-highlighting && \
    sed -i 's/plugins=(git)/plugins=(git z zsh-autosuggestions zsh-syntax-highlighting)/' /root/.zshrc && \
    sed -i 's/ZSH_THEME="robbyrussell"/ZSH_THEME="agnoster"/' /root/.zshrc && \
    chsh -s "$(which zsh)" root

# ============================================================
# conda — copied from stage 0, no architecture-specific download needed
# ============================================================
COPY --from=conda-base /opt/conda /opt/conda

RUN conda config --remove channels defaults 2>/dev/null || true && \
    conda config --add channels ${CONDA_MIRROR}/pkgs/free && \
    conda config --add channels ${CONDA_MIRROR}/pkgs/main && \
    conda config --set show_channel_urls yes && \
    conda config --set channel_priority strict

# ============================================================
# Python dependencies
# ============================================================
RUN printf "[global]\nindex-url = ${PYPI_MIRROR}\n" > /etc/pip.conf

COPY environment-312.yml /tmp/
RUN conda env create -q -f /tmp/environment-312.yml && \
    conda clean -ya && \
    pip cache purge 2>/dev/null || true

# ============================================================
# Shell customization
# ============================================================
RUN conda init zsh && \
    echo "conda activate py312" >> /root/.zshrc && \
    echo "export REPO_URL=\"${REPO_MIRROR}\"" >> /root/.zshrc

# AscendC banner
RUN printf '\n\
echo "\\033[1;36m"\n\
echo " █████╗ ███████╗ ██████╗███████╗███╗   ██╗██████╗  ██████╗"\n\
echo "██╔══██╗██╔════╝██╔════╝██╔════╝████╗  ██║██╔══██╗██╔════╝"\n\
echo "███████║███████╗██║     █████╗  ██╔██╗ ██║██║  ██║██║     "\n\
echo "██╔══██║╚════██║██║     ██╔══╝  ██║╚██╗██║██║  ██║██║     "\n\
echo "██║  ██║███████║╚██████╗███████╗██║ ╚████║██████╔╝╚██████╗"\n\
echo "╚═╝  ╚═╝╚══════╝ ╚═════╝╚══════╝╚═╝  ╚═══╝╚═════╝  ╚═════╝"\n\
echo "\\033[0m"\n' >> /root/.zshrc

# Ascend driver environment
RUN printf '\n[ -f /usr/local/Ascend/driver/bin/setenv.bash ] && source /usr/local/Ascend/driver/bin/setenv.bash\n' \
    >> /root/.zshrc

RUN rm -rf /tmp/*

WORKDIR /root
ENTRYPOINT ["/bin/zsh"]
