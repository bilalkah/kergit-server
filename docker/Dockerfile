FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG BAZELISK_VERSION=1.21.0

# Toolchain & deps
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
      build-essential gcc g++ clang lld cmake ninja-build make pkg-config \
      git curl wget ca-certificates unzip zip tar xz-utils \
      python3 python3-pip python3-venv \
      ccache \
      openssl libssl-dev \
      zlib1g-dev libzstd-dev \
      libuv1-dev libevent-dev libunwind-dev \
      libreadline-dev libncurses-dev libffi-dev \
      libpq-dev \
      iproute2 net-tools iputils-ping procps \
      openssh-client \
      locales tzdata \
      clang-format-20 tmux nano && \
    rm -rf /var/lib/apt/lists/*

# Locale
RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8

# Bazel via Bazelisk
RUN arch=$(uname -m) && \
    case "$arch" in \
      aarch64|arm64)  url="https://github.com/bazelbuild/bazelisk/releases/download/v${BAZELISK_VERSION}/bazelisk-linux-arm64" ;; \
      x86_64|amd64)   url="https://github.com/bazelbuild/bazelisk/releases/download/v${BAZELISK_VERSION}/bazelisk-linux-amd64" ;; \
      *) echo "Unsupported arch: $arch" && exit 1 ;; \
    esac && \
    curl -fsSL "$url" -o /usr/local/bin/bazelisk && \
    chmod +x /usr/local/bin/bazelisk && \
    ln -s /usr/local/bin/bazelisk /usr/local/bin/bazel

# ---- Developer user 'sercom' with writable home, ccache, workspace
RUN useradd -m -s /bin/bash sercom \
    && mkdir -p /etc/sudoers.d \
    && echo "sercom ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/sercom \
    && chmod 0440 /etc/sudoers.d/sercom \
    && mkdir -p /home/sercom/.ccache /home/sercom/workspace \
    && chown -R sercom:sercom /home/sercom

ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    CCACHE_DIR=/home/sercom/.ccache \
    PATH="/usr/lib/ccache:/usr/local/bin:/usr/bin:/bin:$PATH" \
    CC=/usr/lib/ccache/gcc \
    CXX=/usr/∏lib/ccache/g++

# Keep Bazel from reusing any stale auto-config (safe default)
ENV BAZELISK_HOME=/home/sercom/.cache/bazelisk

# Workspace & git safety
WORKDIR /home/sercom/workspace
RUN git config --system --add safe.directory /home/sercom/workspace

USER sercom

# Friendly login
ENTRYPOINT ["bash","-lc","echo 'Dev container ready at /home/sercom/workspace'; exec bash"]
