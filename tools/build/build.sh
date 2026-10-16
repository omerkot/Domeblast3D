#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
docker build --platform linux/arm64 -t domeblast3d-builder -f tools/build/Dockerfile tools/build
build_container=$(docker create --platform linux/arm64 domeblast3d-builder make)
trap 'docker rm -f "$build_container" >/dev/null' EXIT
docker cp src "$build_container:/src/src"
docker cp third_party "$build_container:/src/third_party"
docker cp Makefile "$build_container:/src/Makefile"
docker start -a "$build_container"
[ "$(docker inspect -f '{{.State.ExitCode}}' "$build_container")" = 0 ]
mkdir -p build
docker cp "$build_container:/src/build/domeblast3d.aarch64" build/
