<!-- SPDX-License-Identifier: CC-BY-4.0 -->

OAI Docker/Podman Build and Usage Procedures

**Table of Contents**

[[_TOC_]]

---

# 1. Build Strategy #

For all platforms, the strategy for building docker/podman images is the same:

*  First we create a common shared image `ran-base` that contains:
   -  the latest source files (by using the `COPY` function)
   -  all the means to build an OAI RAN executable
      *  all packages, compilers, ...
      *  especially UHD is installed 
*  Then, from the `ran-base` shared image, we create a shared image `ran-build`
   into which all targets are compiled.
*  Then from the `ran-build` shared image, we can build target images for:
   -  eNB
   -  gNB/DU (with UHD)
   -  gNB/DU (with AW2S), only on RHEL9
   -  lte-UE
   -  nr-UE
   -  nr-cuup
   -  nr-oru
   These target images will only contain:
   -  the generated executable (for example `lte-softmodem`)
   -  the generated shared libraries (for example `liboai_usrpdevif.so`)
   -  the needed libraries and packages to run these generated binaries
   -  Some configuration file templates
   -  Some tools (such as `ping`, `ifconfig`)
* From the `ran-build-fhi72` image, we can build target image for:
   -  gNB/DU (with FHI 7.2)

Note that on every push to develop (i.e., typically when integration branch is merged),
we build all images and push them to [Docker Hub](https://hub.docker.com/u/oaisoftwarealliance)
in [RAN-DockerHub-Push](https://jenkins-oai.eurecom.fr/job/RAN-DockerHub-Push/) pipeline. Images
tagged develop are overwritten, and new images are published with the corresponding integration
week tag.
To pull them, do:

```bash
docker pull oaisoftwarealliance/oai-gnb:develop
docker pull oaisoftwarealliance/oai-nr-ue:develop
docker pull oaisoftwarealliance/oai-enb:develop
docker pull oaisoftwarealliance/oai-lte-ue:develop
docker pull oaisoftwarealliance/oai-gnb-fhi72:develop
docker pull oaisoftwarealliance/oai-nr-cuup:develop
```
Have a look at [this README](../ci-scripts/yaml_files/5g_rfsimulator/README.md) to get some information on how to use the images.

# 2. File organization #

Dockerfiles are named with the following naming convention: `Dockerfile.${target}.${OS-version}`

Targets can be:

-  `base` for an image named `ran-base` (shared image)
-  `build` for an image named `ran-build` (shared image)
-  `build.fhi72` for an image named `ran-build-fhi72`
-  `eNB` for an image named `oai-enb`
-  `gNB` for an image named `oai-gnb`
-  `nr-cuup` for an image named `oai-nr-cuup`
-  `gNB.aw2s` for an image named `oai-gnb-aw2s`
-  `gNB.fhi` for an image named `oai-gnb-fhi72`
-  `lteUE` for an image named `oai-lte-ue`
-  `nrUE` for an image named `oai-nr-ue`
-  `nrORU` for an image named `oai-nr-oru`

The currently-supported OS are:

- `rhel9` for Red Hat Enterprise Linux and Openshift Universal Base Image
- `ubuntu` for Ubuntu 24.04 LTS
- `rocky` for Rocky-Linux 9

For more details regarding the build on an Openshift Cluster, see [OpenShift README](../openshift/README.md).

# 3. Building using `docker` under Ubuntu 24.04 #

## 3.1. Pre-requisites ##

* `git` installed
* `docker-ce` installed
* Pulling `ubuntu:noble` from DockerHub

The docker files in this directory rely on [automatic Docker platform
arguments](https://docs.docker.com/reference/dockerfile/#automatic-platform-args-in-the-global-scope)
`TARGETARCH` and `TARGETPLATFORM` to be defined. This is the case when using
[BuildKit](https://docs.docker.com/build/buildkit/), which is automatically
enabled in newer (v23.0+) docker versions.

If you are running an older version of docker that does not have BuildKit
enabled and do not wish to upgrade to a newer docker version, you have two
possibilities:

1. [Enable BuildKit](https://docs.docker.com/build/buildkit/#getting-started)
   when building: `DOCKER_BUILDKIT=1 docker build ...`
2. You should be able to define these variables manually.
   - x86: `docker build --build-arg TARGETARCH=amd64 --build-arg
     TARGETPLATFORM=linux/amd64 ...`
   - arm: `docker build --build-arg TARGETARCH=arm64 --build-arg
     TARGETPLATFORM=linux/arm64 ...`

## 3.2. Building the shared images ##

There are two shared images: one that has all dependencies, and a second that compiles all targets (eNB, gNB, [nr]UE).

```bash
git clone https://github.com/duranta-project/openairinterface5g.git
cd openairinterface5g
# default branch is develop, to change use git checkout <BRANCH>
docker build --target ran-base --tag ran-base:latest --file docker/Dockerfile.base.ubuntu .
# if you want use USRP, AW2S and RFSimulator radios
docker build --target ran-build --tag ran-build:latest --file docker/Dockerfile.build.ubuntu .
# if you want to use front-haul 7.2 and RFSimulator radios
docker build --tag ran-build-fhi72:latest --file docker/Dockerfile.build.fhi72.ubuntu .
```

After building:

```bash
docker image ls
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
ran-build           latest              f2633a7f5102        1 minute ago        6.81GB
ran-base            latest              5c9c02a5b4a8        1 minute ago        2.4GB
ran-build-fhi72     latest              5190c06dbb4d        1 minute ago    6.86GB
...
```

Note that the steps are identical for `rocky-linux`.

### 3.2.1. Additional build options

This is only available for the Ubuntu version of Dockerfiles.

You can, for example, create a `sanitizer` version of the ran-build image.

```bash
docker build --target ran-build --tag ran-build:latest --file docker/Dockerfile.build.ubuntu --build-arg "BUILD_OPTION=--sanitize" .
```

Currently the `--sanitize` option for `build_oai` enables:

* [Address Sanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)
* [Undefined Behavior Sanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)

After building:

```bash
docker image ls
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
ran-build           latest              f2633a7f5102        1 minute ago        8.78GB
...
```

Note that the image is much bigger.

You can also use this docker build arguments to pass any available option(s) on the `build-oai` script.

## 3.3. Building any target image ##

For example, the eNB:

```bash
docker build --target oai-enb --tag oai-enb:latest --file docker/Dockerfile.eNB.ubuntu .
```

To build gNB/DU with 7.2 fronthaul support:

```bash
docker build --target oai-gnb-fhi72 --tag oai-gnb-fhi72:latest --file docker/Dockerfile.gNB.fhi72.ubuntu  .
```

After a while:

```
docker image ls
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
oai-enb             latest              25ddbd8b7187        1 minute ago        516MB
ran-build           latest              f2633a7f5102        1 hour ago          6.81GB
ran-base            latest              5c9c02a5b4a8        1 hour ago          2.4GB
```

Do not forget to remove the temporary image:

```
docker image prune --force
```

Note that the steps are identical for `rocky-linux`.

If you have used the sanitizer option, then you should also pass it when building the target image:

```bash
docker build --target oai-gnb --tag oai-gnb:latest --file docker/Dockerfile.gNB.ubuntu --build-arg "BUILD_OPTION=--sanitize" .
```

Normally the target image will be around 200 Mbytes bigger.

# 4. Building using `podman` under Red Hat Entreprise Linux 9.X #

Analogous to the above steps:

```bash
podman build --target ran-base --tag ran-base:latest --file docker/Dockerfile.base.rhel9 .
# if you want use USRP, AW2S and RFSimulator radios
podman build --target ran-build --tag ran-build:latest --file docker/Dockerfile.build.rhel9 .
# if you want to use front-haul 7.2 and RFSimulator radios
podman build --tag ran-build-fhi72:latest --file docker/Dockerfile.build.fhi72.rhel9 .
```

## 4.1 Building target images

For example, the eNB:

```bash
podman build --target oai-enb --tag oai-enb:latest --file docker/Dockerfile.eNB.rhel9 .
```

To build gNB/DU with 7.2 fronthaul support:

```bash
podman build --target oai-gnb-fhi72 --tag oai-gnb-fhi72:latest --file docker/Dockerfile.gNB.fhi72.rhel9  .
```

# 5. Running modems using `docker` under Ubuntu #

The easiest is to run them from a `docker-compose` file, which is used by the
CI to test OAI. Some folders under `ci-scripts/yaml_files` have a README that
you can follow. For 5G, the easiest is to start with the RFsimulator, as
described in [this README](../ci-scripts/yaml_files/5g_rfsimulator/README.md)
(you would of course use your own images instead of downloading them from
Docker hub).

For an example using a B210, please refer to [this `docker-compose`
file](../ci-scripts/yaml_files/sa_b200_gnb/docker-compose.yml).

It is also possible to mount your own configuration file. The following
docker-compose file can be used to start a gNB using a B210 and your own
config, located at `/tmp/gnb.conf`:

```yaml
services:
    gnb_mono_tdd:
        image: oai-gnb:latest
        container_name: sa-b200-gnb
        cap_drop:
            - ALL
        cap_add:
            - SYS_NICE
            - IPC_LOCK
        ulimits:
          core: -1 # for core dumps
        environment:
            USE_B2XX: 'yes'
            USE_ADDITIONAL_OPTIONS: --RUs.[0].sdr_addrs serial=30C51D4 --telnetsrv --telnetsrv.shrmod ci --continuous-tx --log_config.global_log_options level,nocolor,time,line_num,function
        devices:
            - /dev/bus/usb/:/dev/bus/usb/
        volumes:
            - ../../conf_files/gnb.sa.band78.51prb.usrpb200.conf:/opt/oai-gnb/etc/gnb.conf
        # for performance reasons, we use host mode: in bridge mode, we have
        # unacceptable DL TCP performance. However, the whole point of
        # containerization is to not be in host mode, so update this to macvlan
        # later.
        network_mode: "host"
        #entrypoint: /bin/bash -c "sleep infinity"
        healthcheck:
            # pgrep does NOT work
            test: /bin/bash -c "ps aux | grep -v grep | grep -c softmodem"
            interval: 10s
            timeout: 5s
            retries: 5
```

You should also change the `image` to the right image name and tag of the gNB
you are using. Start like this:
```bash
docker-compose up    # gNB in foreground
docker-compose up -d # gNB in background
```
Stop it like this (in both cases):
```bash
docker-compose down
```
