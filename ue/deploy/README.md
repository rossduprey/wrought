# deploy/ — cooking and shipping the wrought dedicated server

A UE dedicated server is a **cooked binary, not source**. Nothing here runs until you've
cooked one on a machine with the engine installed. The whole pipeline:

## 1. Cook the Linux server (on a machine with the engine)

```
RunUAT BuildCookRun -project=<path>/Wrought.uproject \
  -noP4 -platform=Linux -clientconfig=Development -serverconfig=Development \
  -server -noclient -cook -stage -pak -archive -archivedirectory=<out>
```

This needs a **boot map** to cook — see the repo `README.md` ("Dedicated server"): make
`Content/Maps/Hearth` once in the editor. The result is `<out>/LinuxServer/`.

## 2. Containerize

From `<out>` (the dir that contains `LinuxServer/`):

```
docker build -f <repo>/ue/deploy/Dockerfile -t <your-registry>/wrought-server:latest .
docker push <your-registry>/wrought-server:latest
```

## 3. Run

Anywhere that can pull the image:

```
docker run --rm -p 7777:7777/udp <your-registry>/wrought-server:latest
```

or hand it to your orchestrator as a UDP service on `7777`. Point clients at the host and
they hit the seam in `WroughtPlayerController` — crosshair hit up, assay back.

> If you deploy this on a private LAN/cluster, keep the registry name, node placement, and
> hostname in that environment's **own** (private) config — not in this public repo.
