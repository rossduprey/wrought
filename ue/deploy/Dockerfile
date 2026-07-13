# Wraps a cooked Unreal Linux dedicated server into a runnable image.
#
# The build CONTEXT must be the cook's archive directory — the one passed to
# -archivedirectory in the WroughtServer cook (see ../README.md → "Dedicated server").
# That directory contains a LinuxServer/ folder with WroughtServer.sh + the binary + the
# cooked content. From there:
#
#   docker build -f <repo>/ue/deploy/Dockerfile -t <your-registry>/wrought-server:latest .
#   docker push <your-registry>/wrought-server:latest
#   docker run --rm -p 7777:7777/udp <your-registry>/wrought-server:latest
#
# The image is content-only; it pulls in just the shared libs a cooked UE server needs.
FROM ubuntu:22.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends libc6 libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /server
COPY LinuxServer/ /server/
RUN chmod +x /server/WroughtServer.sh || true

# UE dedicated-server default port. Match whatever the host maps / the orchestrator opens.
EXPOSE 7777/udp

# WroughtServer.sh execs the real binary. -log sends the server log to stdout so a
# container log collector captures it. The boot map + game mode come from
# Config/DefaultEngine.ini, so no map has to be named here.
ENTRYPOINT ["/server/WroughtServer.sh"]
CMD ["-server", "-port=7777", "-log"]
