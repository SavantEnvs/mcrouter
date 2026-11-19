# syntax=docker/dockerfile:1.7
#
# Commit image for mcrouter (small form, on the org C/C++ base).
#
# mcrouter's CMake build needs folly + fizz + wangle + mvfst + fbthrift at the revisions pinned in
# build/deps/github_hashes/ (getdeps would fetch them). This image builds each once from that pinned
# revision into /opt/toolchains/deps/<name>-<rev> (static, uninstrumented, -g -gdwarf-3, source kept
# in src/), and build.sh runs upstream's own CMake build against them offline.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
      cmake ninja-build pkg-config curl ca-certificates \
      libboost-context-dev libboost-filesystem-dev libboost-program-options-dev libboost-regex-dev \
      libboost-system-dev libboost-thread-dev libdouble-conversion-dev libfast-float-dev libfmt-dev \
      libgoogle-glog-dev libgflags-dev libevent-dev liblz4-dev libzstd-dev libsnappy-dev zlib1g-dev \
      libbz2-dev liblzma-dev libssl-dev libsodium-dev libunwind-dev libdwarf-dev libiberty-dev \
 && rm -rf /var/lib/apt/lists/*

# folly, pinned to upstream's build/deps/github_hashes/facebook/folly-rev.txt; static, uninstrumented,
# -g -gdwarf-3, source kept in $d/src (SPEC §6.2 item 19).
ARG FOLLY_REV=24e09e96427ec011a5e2a402a35b4182e5a51f51
ARG FOLLY_SHA256=8f7b9c7ef52a52abf4bee56cfdae78f09c9584260b3ad2f720e671e28f62a756
RUN d=/opt/toolchains/deps/folly-${FOLLY_REV} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/facebook/folly/archive/${FOLLY_REV}.tar.gz \
 && echo "${FOLLY_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src" -B /tmp/dep-build -G Ninja \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DFOLLY_FETCH_BOOST=OFF -DFOLLY_BUILD_OPENSSL=OFF \
      -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" \
 && ln -s "folly-${FOLLY_REV}" /opt/toolchains/deps/folly

# The rest of upstream's dependency chain (fizz, wangle, mvfst, fbthrift), each pinned to the
# revision in build/deps/github_hashes/, static and uninstrumented like folly.
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends libgtest-dev libgmock-dev \
      libxxhash-dev libsystemd-dev bison flex ragel python3 \
 && rm -rf /var/lib/apt/lists/*
ENV DEPS_PREFIX=/opt/toolchains/deps/folly;/opt/toolchains/deps/fizz;/opt/toolchains/deps/wangle;/opt/toolchains/deps/mvfst;/opt/toolchains/deps/fbthrift
ARG FIZZ_REV=6c95d45a39641b24026335cea24753fd4562c37b
ARG FIZZ_SHA256=a1137e1daea2f65c9122abfc7f7fb191d405d05ec3b24f3b84cc0d5e52c9916c
RUN d=/opt/toolchains/deps/fizz-${FIZZ_REV} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/facebookincubator/fizz/archive/${FIZZ_REV}.tar.gz \
 && echo "${FIZZ_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src/fizz" -B /tmp/dep-build -G Ninja \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
      -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON  \
      -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" \
 && ln -s "fizz-${FIZZ_REV}" /opt/toolchains/deps/fizz
ARG WANGLE_REV=d9817a72ca72446aac6bdc451b480a8e881d4888
ARG WANGLE_SHA256=2de4ae0895e0598e723be2c6df876db29e205a20f8ef1f7a47d8eb5169d3603f
RUN d=/opt/toolchains/deps/wangle-${WANGLE_REV} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/facebook/wangle/archive/${WANGLE_REV}.tar.gz \
 && echo "${WANGLE_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src/wangle" -B /tmp/dep-build -G Ninja \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
      -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON  \
      -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" \
 && ln -s "wangle-${WANGLE_REV}" /opt/toolchains/deps/wangle
ARG MVFST_REV=84a6f11b8afca317cc07d4ec6d0dd817e3c0858c
ARG MVFST_SHA256=3bcd63c5bbfbe9c712a0ca54959136d273bdf17ad261e1de169bc79e802bec86
RUN d=/opt/toolchains/deps/mvfst-${MVFST_REV} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/facebook/mvfst/archive/${MVFST_REV}.tar.gz \
 && echo "${MVFST_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src" -B /tmp/dep-build -G Ninja \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
      -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON  \
      -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" \
 && ln -s "mvfst-${MVFST_REV}" /opt/toolchains/deps/mvfst
ARG FBTHRIFT_REV=5c69d38a97ed3730a47d4c1dbdad9c0813bb319d
ARG FBTHRIFT_SHA256=5637fa8b28ada831f46ddfede8543126c9ea6716a739a6c468ec8eb8c18ea751
RUN d=/opt/toolchains/deps/fbthrift-${FBTHRIFT_REV} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz https://github.com/facebook/fbthrift/archive/${FBTHRIFT_REV}.tar.gz \
 && echo "${FBTHRIFT_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src" -B /tmp/dep-build -G Ninja \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH="$DEPS_PREFIX" \
      -DCMAKE_CXX_STANDARD=20 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DTHRIFT_TESTS=OFF -Denable_tests=OFF \
      -DCMAKE_C_FLAGS="-g -gdwarf-3" -DCMAKE_CXX_FLAGS="-g -gdwarf-3" \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" \
 && ln -s "fbthrift-${FBTHRIFT_REV}" /opt/toolchains/deps/fbthrift

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
