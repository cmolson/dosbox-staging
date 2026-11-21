# Virtual display with software OpenGL, plus xdotool and ffmpeg.
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        xvfb xauth libgl1-mesa-dri libgl1 libegl1 libasound2 \
        xdotool imagemagick x11-utils ffmpeg \
    && rm -rf /var/lib/apt/lists/*
