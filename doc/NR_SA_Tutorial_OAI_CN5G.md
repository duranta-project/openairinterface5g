<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# OAI 5G NR SA tutorial: setting up OAI CN5G

**Table of Contents**

[[_TOC_]]

##  1. Scenario
In this tutorial we describe how to configure and run a 5G end-to-end setup with OAI CN5G, OAI gNB and COTS UE.

Minimum hardware requirements:

- Laptop/Desktop/Server for OAI CN5G and OAI gNB

    - Operating System: [Ubuntu Desktop 26.04.1 LTS, Intel or AMD 64-bit architecture](https://releases.ubuntu.com/resolute/ubuntu-26.04.1-desktop-amd64.iso)
    - CPU: 8 cores x86_64 @ 3.5 GHz
    - RAM: 32 GB

## 2. OAI CN5G

### 2.1 OAI CN5G pre-requisites

```bash
sudo apt install -y git net-tools putty

# https://docs.docker.com/engine/install/ubuntu/
sudo apt update
sudo apt install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Add your username to the docker group, otherwise you will have to run in sudo mode.
sudo usermod -a -G docker $(whoami)
reboot
```

### 2.2 OAI CN5G configuration files
Download and copy configuration files:
```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/duranta-project/openairinterface5g.git /tmp/openairinterface5g -b develop
cd /tmp/openairinterface5g
git sparse-checkout set doc/tutorial_resources/oai-cn5g
mv doc/tutorial_resources/oai-cn5g ~/oai-cn5g
cd && rm -rf /tmp/openairinterface5g
```

### 2.3 Pull OAI CN5G docker images

```bash
cd ~/oai-cn5g
docker compose pull
```

## 3. Run OAI CN5G
### 3.1 Start OAI CN5G
```bash
cd ~/oai-cn5g
docker compose up -d
```

### 3.2 Stop OAI CN5G
```bash
cd ~/oai-cn5g
docker compose down
```

## 4. Run 5G NR SA end-to-end setup with OAI gNB
### 4.1 Testing with COTS UE
Please check this link:
[Testing with OAI gNB and COTS UE](NR_SA_Tutorial_COTS_UE.md)

### 4.2 Testing with OAI nrUE
Please check this link:
[Testing with OAI gNB and OAI nrUE](NR_SA_Tutorial_OAI_nrUE.md)
