<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# How to use commercial SIM cards with the NR UE

Using a PL2303-based SIM card reader (as the one sold by Laurent Thomas'
Open Cells company), you can use a commercial SIM card with NR UE.

Insert the reader in your computer. It will create `/dev/ttyUSB0` (name
may be different, analyse the output of `sudo dmesg`).

Do: `sudo chmod 666 /dev/ttyUSB0`

Then in `ran_build/build` do: `ninja remote_sim_server`

Then run the SIM card server.

In `ran_build/build` type:

```
./common/utils/remote_sim/server/remote_sim_server -pin [your PIN code]
```

`-pin [your PIN code]` may be omitted if no PIN is configured.

The output should be similar to:

```
use card /dev/ttyUSB0
IMSI 999990000000112
listen on 0.0.0.0:4098, wait for a client
```

For more help about this program, use `--help`.

Once it is running, run the NR UE as usual. Add the option
`--remote-sim [server]` to the command line and the NR UE will connect to
the SIM card server.

`[server]` is either `<IP address>` or `<IP address>:<TCP port>`. If
`<TCP port>` is not provided, the default value 4098 is used.

Technically, the NR UE only contacts the SIM card reader to do authentication.
All other parameters (IMSI, etc.) still need to be given to the NR UE as
usual (configuration file, command line).

The SIM card server prints the IMSI at startup for information only.

It is also possible to run the SIM card server in the directory
`common/utils/remote_sim/server` where you do:

```
make
./simcard -pin [your PIN]
```
