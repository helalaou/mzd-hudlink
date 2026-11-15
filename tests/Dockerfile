# Test environment for the USB scripts.
#
# Debian with BusyBox, the shell and tools the head unit's scripts run with,
# and a static BusyBox for the fake dynamic loader. The scripts set loader
# variables (LD_PRELOAD, LD_TRACE_LOADED_OBJECTS) for their library tests; a
# static interpreter ignores them, so they reach only the fake. The static
# build is unpacked beside the normal one rather than installed, because it
# also runs its own applets in place of the fake programs on the PATH.
# Built for the machine's own architecture: BusyBox re-runs itself for some
# commands, which an emulated container can break.
#
#   docker build --pull -t hudlink-test tests
#
# tools/test.sh builds this image when it is missing.

FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends busybox \
 && cd /tmp \
 && apt-get download busybox-static \
 && dpkg-deb -x busybox-static_*.deb /tmp/busybox-static \
 && install -D -m 755 /tmp/busybox-static/bin/busybox /opt/busybox-static/busybox \
 && rm -rf /tmp/busybox-static /tmp/busybox-static_*.deb /var/lib/apt/lists/*
