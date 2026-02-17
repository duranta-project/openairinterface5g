# OAI-SL-MultiUE-Broker



## RFSim Broker based Multiple UEs Test 

This part test is based on ‘sl-eurecom4’ branch on openairinterface5g.
(https://gitlab.eurecom.fr/oai/openairinterface5g.git)


## To start

* First go to the correct repository :
```
cd openairinterface5g/cmake_targets
```
* To install zeroMQ : 
```
apt-get install libzmq3-dev
```
* Then start compilation : 
```
sudo ./build_oai --nrUE -w SIMU --cmake-opt -DENABLE_T_TRACER=OFF
```
* Or, to start a clean start compilation, do the clean up first then compile : 
```
sudo ./build_oai -c -C  

sudo ./build_oai -I --cmake-opt -DENABLE_T_TRACER=OFF
```
* Now we need to create three UEs through namespaces: 
```
cd openairinterface5g/cmake_targets
sudo ./multi-ue.sh -c1 -c2 -c3
```



 
## Start RFSim for Multiple UEs through Broker


### Launch Broker (outside namespace):
```
cd ran_build/build/
./broker
```

### Launch SYNC-REF (namespace 1):
```
sudo ./multi-ue.sh -o1
cd ran_build/build/
sudo RFSIMULATOR=server ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/sl_sync_ref.conf --sl-mode 2 --sa --sync-ref --brokerip 10.201.1.100
```

### Launch UE2 (namespace 2):
```
sudo ./multi-ue.sh -o2
cd ran_build/build/
sudo ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/ue1.conf --sl-mode 2 --sa --brokerip 10.202.1.100 --device_id 1 | sudo tee $HOME/sl-oai-release4/rat-selection/logs/sl.log 
```

### Launch UE3 (namespace 3):
```
sudo ./multi-ue.sh -o3
cd ran_build/build/
 sudo ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/ue2.conf --sl-mode 2 --sa --brokerip 10.203.1.100 --device_id 2 | sudo tee $HOME/sl-oai-release4/rat-selection/logs/sl.log
```

Optional : You can now quickly check the connection to sync-ref from two UEs : 
```
ping -I oaitun_ue2 10.0.0.1
ping -I oaitun_ue3 10.0.0.1
```

Note: when restart the simulation, you might need to kill everything first by the following command : 
``` 
sudo pkill -f nr-uesoftmodem 
sudo pkill -f broker
sudo pkill -f rfsimulator
```

##  Monitor route with Babel and Wireshark
You might need to first install Babel onto the local machine

```
sudo apt update
sudo apt install babeld
```

### Launch SYNC-REF (namespace 1):
First go into the correct namespace : 
```
sudo ./multi-ue.sh -o1
```

In order to have a readable address at the logside, we'll hardcode the IP address through following command, so sync-ref will be translate as 'fe80::1/64'
```
sudo ip netns exec ue1 ip -6 addr add fe80::1/64 dev oaitun_ue1 nodad  
```
Then launch Babel on sync-ref : 
```
ip netns exec ue1 bash -lc '
set -e
cat > /tmp/babeld-ue1.conf <<EOF
interface oaitun_ue1
redistribute local deny
pid-file /tmp/babeld-ue1.pid
EOF
babeld -d 1 -c /tmp/babeld-ue1.conf
'
```
For now, as no other UE is connected, sync-ref will simply print its local id.

### Launch UE2 (namespace 2):
First go into the correct namespace : 
```
sudo ./multi-ue.sh -o2
```

UE2 will be allocated address as 'fe80::2/64'
```
sudo ip netns exec ue2 ip -6 addr add fe80::2/64 dev oaitun_ue2 nodad
```
Then launch Babel on UE2 : 
```
ip netns exec ue2 bash -lc '
set -e
cat > /tmp/babeld-ue2.conf <<EOF
interface oaitun_ue2
redistribute local deny
pid-file /tmp/babeld-ue2.pid
EOF
babeld -d 1 -c /tmp/babeld-ue2.conf
'
```

### Launch UE3 (namespace 3):
First go into the correct namespace : 
```
sudo ./multi-ue.sh -o3
```

UE3 will be allocated address as 'fe80::3/64'
```
sudo ip netns exec ue3 ip -6 addr add fe80::3/64 dev oaitun_ue3 nodad

```
Then launch Babel on UE3 : 
```
ip netns exec ue3 bash -lc '
set -e
cat > /tmp/babeld-ue3.conf <<EOF
interface oaitun_ue3
redistribute local deny
pid-file /tmp/babeld-ue3.pid
EOF
babeld -d 1 -c /tmp/babeld-ue3.conf
'

```

Note : On babel, it will show neighbour connection with 'Neighbour ... rxcost 96 txcost 65535...', when it's 96, it means perfect connection, 65535 means a bad connection, if the connection is bad when generating the UE3, try to kill and restart again.

### Wireshark monitor
Within the each namespace, you can start wireshark with filter on each node to check the Babel messages (example as for sync-ref, please update the oaitun_ueX name for each UE): 
```
sudo wireshark -i oaitun_ue1 -k &
```
You should be able to see 'Babel Hello' message, and 'Babel Hello ihu'. ihu representing 'I hear you'.
When there are two UEs connecting to sync-ref at the same time, you should be able to see 'Babel Hello ihu ihu'


## Enable TAP in RFSim for Multiple UEs
This part is to switch from default TUN to TAP. 
The baseline is, by default, system is running through TUN. In order to enable TAP, when you run each node simply add a flag at the begining: " OAI_TUNTAP_MODE=tap".

Optional : To disable the TAP (in case of an uncleaned restart...), do the following : 
```
unset OAI_TUNTAP_MODE
```
### Launch Broker (outside namespace):
```
cd ran_build/build/
./broker
```
### Launch SYNC-REF (namespace 1):
```
sudo ./multi-ue.sh -o1
cd ran_build/build/
sudo OAI_TUNTAP_MODE=tap RFSIMULATOR=server ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/sl_sync_ref.conf --sl-mode 2 --sa --sync-ref --brokerip 10.201.1.100
```

### Launch UE2 (namespace 2):
```
sudo ./multi-ue.sh -o2
cd ran_build/build/
sudo OAI_TUNTAP_MODE=tap ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/ue1.conf --sl-mode 2 --sa --brokerip 10.202.1.100 --device_id 1 | sudo tee $HOME/sl-oai-release4/rat-selection/logs/sl.log
```

### Launch UE3 (namespace 3):
```
sudo ./multi-ue.sh -o3
cd ran_build/build/
sudo OAI_TUNTAP_MODE=tap ./nr-uesoftmodem --rfsim -O ../../../targets/PROJECTS/NR-SIDELINK/CONF/ue2.conf --sl-mode 2 --sa --brokerip 10.203.1.100 --device_id 2 | sudo tee $HOME/sl-oai-release4/rat-selection/logs/sl.log
```

## Check connection with Batman

### Start Batman on sync-ref (namespace 1):
```
sudo ./multi-ue.sh -o1
ip link set oaitun_ue1 address 02:00:00:00:00:01
sudo batctl if add oaitun_ue1
```

### Start Batman on UE2 (namespace 2):
```
sudo ./multi-ue.sh -o2
ip link set oaitun_ue1 address 02:00:00:00:00:02
sudo batctl if add oaitun_ue2
```


### Start Batman on UE3 (namespace 3):
```
sudo ./multi-ue.sh -o3
ip link set oaitun_ue1 address 02:00:00:00:00:03
sudo batctl if add oaitun_ue3
```
### Batman command
To check the neighbour : 
```
batctl n
```
To check the routing table : 
```
batctl o
```

###Start wireshark with filter
You can check the current packets exchange with wireshark, please update the name 'oaitun_ueX' accordingly. 
```
wireshark -k -i oaitun_ueX -Y "eth.type == 0x4305"
```

