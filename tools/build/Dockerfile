FROM --platform=linux/arm64 debian:12-slim
RUN apt-get update && apt-get install -y --no-install-recommends g++ make pkg-config libsdl2-dev libgles2-mesa-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /src
