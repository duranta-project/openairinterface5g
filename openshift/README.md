<!-- SPDX-License-Identifier: CC-BY-4.0 -->

OpenShift Build and Usage Procedures

[[_TOC_]]

# 1. Introduction

## General

What follows is a tutorial on how to build images on an OpenShift cluster in a three-step process
(base image, build artifact image, target image). Note that all image
streams/build configs assume the OC project/namespace `oaicicd-ran`.

If you want to regenerate images from modified sources, you have to rebuild
the ran-build image. If you reinstalled dependencies, you have to restart from
ran-base.

You first need to login. Do:

```bash
oc login -u <username> -p <password> --server <url>
```

Create a project/namespace where you will create your images

```bash
oc new-project oaicicd-ran
```

**NOTE**: If you change the project name then you have to change it in all the `yaml` files.

# 2. Build of `base` shared image

Create an image stream and build config that specify image properties and build parameters, then start the build:

```bash
oc create -f openshift/ran-base-is.yaml
oc create -f openshift/ran-base-bc.yaml
oc start-build ran-base --from-file=<oai-repo-directory> --follow
```

The `--from-file=<oai-repo-directory>` uploads the repository (from which the
build is done) as a binary blob. It is therefore possible to make merges or
other (local) modifications and build an image.

# 3. Build of `build` shared image

The same as for the `base` image.

```bash
oc create -f openshift/ran-build-is.yaml
oc create -f openshift/ran-build-bc.yaml
oc start-build ran-build --from-file=<oai-repo-directory> --follow
```

# 4. Build of a target image

The same as for the `base` image:

```bash
oc create -f openshift/oai-gnb-is.yaml
oc create -f openshift/oai-gnb-bc.yaml
oc start-build oai-gnb --from-file=<oai-repo-directory> --follow
```

# 5. Retrieval of Build Container Images and Build Artifacts

Build artifacts, such as build logs, can be retrieved by checking the logs of
the individual build steps. For example, for the `ran-build` step, you can do:
```bash
oc logs ran-build-1-build
```
where `ran-build-1-build` is the name of the `ran-build` build (type `oc get
pods` to get a list of all pods).

It is also possible to retrieve the build logs from a running pod. Currently,
this is only used for the `ran-base` step (the others using a python script to
scrape it from stdout):
```bash
oc create -f openshift/ran-base-log-retrieval.yaml
oc rsync ran-base-log-retrieval:/oai-ran/cmake_targets/log cmake_targets/log/ran-base
oc delete -f openshift/ran-base-log-retrieval.yaml
```

The images are uploaded into the OpenShift image registry. To download, login
using podman, then pull an image:
```bash
oc whoami -t | sudo podman login -u <username> --password-stdin <url> --tls-verify=false
sudo podman pull <url>/oaicicd-ran/<image>:<tag> --tls-verify=false
sudo podman logout <url>
```
The `<image>` could be `oai-gnb`, and the `<tag>` `ci-temp`.

# 6. Deployment using HELM Charts

Helm charts are located under `charts`. Assuming that the image is in the image
registry, the physims could be deployed as shown in the following steps:

```bash
grep -rl OAICICD_PROJECT ./charts/ | xargs sed -i -e "s#OAICICD_PROJECT#oaicicd-ran#" # select the correct project
sed -i -e "s#TAG#ci-temp#g" ./charts/physims/values.yaml                              # select the correct tag
helm install physim ./charts/physims/                                                 # deploy
oc get pods                                                                           # get the list of deployed containers
oc logs <pod>                                                                         # inspect the logs of a pod
helm uninstall physim                                                                 # undeploy
```
