# Runtime for JieLi's Linux Pi32v2 toolchain (x86-64 binaries). The image
# contains no JieLi software: h9cc mounts your extracted toolchain read-only
# at /opt/jieli. On Apple Silicon, run it under Colima with Rosetta
# (colima start --vm-type vz --vz-rosetta).
FROM --platform=linux/amd64 debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 make libstdc++6 zlib1g libtinfo6 \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
