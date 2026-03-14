FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG BAZELISK_VERSION=1.21.0
ARG BAZEL_VERSION=8.5.1

# Toolchain & deps
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
      build-essential gcc g++ clang lld cmake ninja-build make pkg-config \
      git curl wget ca-certificates unzip zip tar xz-utils \
      python3 python3-pip python3-venv \
      ccache \
      sudo \
      openssl libssl-dev \
      zlib1g-dev libzstd-dev \
      libuv1-dev libevent-dev libunwind-dev \
      libreadline-dev libncurses-dev libffi-dev \
      libpq-dev \
      iproute2 net-tools iputils-ping procps \
      openssh-client \
      locales tzdata \
      clang-format tmux nano && \
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

RUN mkdir -p /root/.cache/bazelisk && \
    BAZELISK_HOME=/root/.cache/bazelisk USE_BAZEL_VERSION=${BAZEL_VERSION} bazel --version

RUN mkdir -p /root/.ccache /root/workspace

ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    CCACHE_DIR=/root/.ccache \
    PATH="/usr/lib/ccache:/usr/local/bin:/usr/bin:/bin:$PATH" \
    CC=/usr/lib/ccache/gcc \
    CXX=/usr/lib/ccache/g++

# Keep Bazel from reusing any stale auto-config (safe default)
ENV BAZELISK_HOME=/root/.cache/bazelisk \
    USE_BAZEL_VERSION=${BAZEL_VERSION}

# Workspace & git safety
WORKDIR /root/workspace
RUN git config --system --add safe.directory /root/workspace

# Friendly login
ENTRYPOINT ["bash","-lc","echo 'Dev container ready at /root/workspace'; exec bash"]
