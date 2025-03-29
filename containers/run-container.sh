#!/bin/bash

podman run \
    --ipc=host \
    --user root \
    --env XAUTHORITY=/tmp/xauth \
    --volume "$XAUTHORITY:/tmp/xauth" \
    --env DISPLAY="$DISPLAY" \
    --env LIBGL_ALWAYS_SOFTWARE=1 \
    --env GALLIUM_DRIVER=virpipe \
    --volume /tmp/.X11-unix/:/tmp/.X11-unix/ \
    --volume /tmp/.virgl_test:/tmp/.virgl_test \
    --annotation virgl=enabled \
    --rm -it $1
