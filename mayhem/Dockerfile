# syntax=docker/dockerfile:1.7
# Commit image for Drawpile: builds the drawdance C engine (libcommon/libmsg/libengine) through
# upstream's own CMake (server configuration, Qt6 Core/Network/WebSockets from apt) with ASan+UBSan,
# links the libFuzzer harnesses in mayhem/harnesses against it, and builds upstream's drawdance
# unit tests with normal flags for mayhem/test.sh.
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
        cmake ninja-build pkg-config \
        qt6-base-dev qt6-websockets-dev \
        libzstd-dev zlib1g-dev \
        libzip-dev libwebp-dev qt6-svg-dev qt6-multimedia-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools libgl-dev \
 && rm -rf /var/lib/apt/lists/*
USER mayhem

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
