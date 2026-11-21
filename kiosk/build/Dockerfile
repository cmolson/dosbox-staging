# Same toolchain as the official Linux release build.
FROM ubuntu:22.04

ARG VCPKG_BASELINE=6283825b81bb60f952af1d0703638df1de611243
ENV DEBIAN_FRONTEND=noninteractive

COPY packages/ubuntu-22.04-apt.txt /tmp/apt.txt
RUN apt-get update && apt-get install -y --no-install-recommends \
        $(cat /tmp/apt.txt) \
        g++-12 gcc-12 git curl ca-certificates zip unzip tar pkg-config \
        ninja-build python3 python3-pip python3-venv jq nasm bison flex \
        linux-libc-dev make file libltdl-dev \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir "cmake>=3.25,<4"

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && cd /opt/vcpkg && git checkout "$VCPKG_BASELINE" \
    && ./bootstrap-vcpkg.sh -disableMetrics \
    && chmod -R a+rwX /opt/vcpkg

ENV VCPKG_ROOT=/opt/vcpkg \
    CC=gcc-12 \
    CXX=g++-12
