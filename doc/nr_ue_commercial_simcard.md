<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# How to use commercial SIM cards with the NR UE

## With a PL203-based SIM card reader

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

You can run the SIM card server on a remote computer. Note that the
connection between the SIM card server and the NR UE is not secure, it is
a simple TCP socket, anyone can connect. For security, it is possible to
use an SSH tunnel between the machine running the SIM card server and the
machine running the NR UE, or other IP security mechanisms (like IPsec for
example). Details are out of scope of this documentation.

## With a PC/SC-compatible SIM card reader

It is also possible to use a PC/SC-compatible SIM card reader. And even
a soft-SIM (like [swsim](https://github.com/tomasz-lisowski/swsim) or
[onomondo-uicc](https://github.com/onomondo/onomondo-uicc)) that
interfaces with `pcscd`.

Before compiling `remote_sim_server` as explained above you need to
install `libpcsclite`. On a Debian system (and derivatives, such as
Ubuntu), this is the package `libpcsclite-dev`. To install it, do:

```
sudo apt install libpcsclite-dev
```

Then compile as explained above.

To verify that `remote_sim_server` supports PC/SC mode, run it with
`--help`. The output should be similar to the following:

```
generic options:
    -pin <PIN code (if absent: don't use PIN)>
    -d                   print debug logs
    -q                   disable all logs
options for PL2303 mode (the default mode):
    <card filename>      use this card (default /dev/ttyUSB0)
    -ip <listening IP (default 0.0.0.0)>
    -port <listening port (default 4098)>
options for PC/SC mode:
    -pcsc               mandatory, use PC/SC instead of PL2303
    -c <card ID (default 0)>
    -l                  list available PC/SC cards and exit
```

If the compilation failed to detect the presence of `libpcsclite` on your
system, the output will be similar to this:

```
generic options:
    -pin <PIN code (if absent: don't use PIN)>
    -d                   print debug logs
    -q                   disable all logs
options for PL2303 mode (the default mode):
    <card filename>      use this card (default /dev/ttyUSB0)
    -ip <listening IP (default 0.0.0.0)>
    -port <listening port (default 4098)>
PC/SC mode not supported, install libpcsclite-dev and recompile
```

How to solve the problem is dependant on your system. We cannot provide
instructions for all systems.

Be also sure to compile from a fresh repository after installing
`libpcsclite`, otherwise when running `remote_sim_server` you may use a
version of the program that does not support PC/SC.

After a successful compilation, to run the program in PC/SC-mode you first
need to install and run the `pcscd` daemon. On Debian, the package to install
is `pcscd`. After installing it, it should then run in the background, as a
daemon. It is also possible to run it in a terminal, to see some logs. Stop
the daemon (it depends on your system, try `sudo service pcscd stop`, or
refer to the documentation of your system if this command fails to stop
the service `pcscd`).

To run it in a terminal, the following command is good, logging APDU
exchanges between the SIM card and the SIM card server.


```
sudo /usr/sbin/pcscd -f -T --disable-polkit -a

```

`--disable-polkit` is needed if `remote_sim_server` fails to connect to
`pcscd`. Alternatively, you can configure `pcscd` to accept connections
from the user running `remote_sim_server`. Or use `sudo` to run
`remote_sim_server`.

To configure `pcscd` to accept connections from the user running
`remote_sim_server`, refer to the documentation of `pcscd`.

Note that you can also configure the daemon-mode of `pcscd` to use
`--disable-polkit`. Refer to the documentation of `pcscd` and the
documention of your system to find out how to do that.

Once `pcscd` is running, you need to find the SIM card reader that you
want to use.

In `ran_build/build`, run the following command:
```
./common/utils/remote_sim/server/remote_sim_server -pcsc -l
```

On a setup with both `swsim` and a physical SIM reader present, this is
the output:

```
use PC/SC mode
9 PCSC readers found
reader 0 [no SIM]: virtual simcard 00 00
reader 1 [no SIM]: virtual simcard 00 01
reader 2 [no SIM]: virtual simcard 00 02
reader 3 [no SIM]: virtual simcard 00 03
reader 4 [no SIM]: virtual simcard 00 04
reader 5 [no SIM]: virtual simcard 00 05
reader 6 [no SIM]: virtual simcard 00 06
reader 7 [no SIM]: virtual simcard 00 07
reader 8 [SIM inserted]: HID Global OMNIKEY 6121 Smart Card Reader [OMNIKEY 6121 Smart Card Reader] 00 00
```

The physical SIM card reader is identified as reader 8. It has a SIM card
inserted.

On this setup, to use the physical SIM card reader (which is number 8),
the following command is used:

```
./common/utils/remote_sim/server/remote_sim_server -pcsc -c 8 -pin [your PIN]
```

On your setup, the reader may very well be assigned another number. Adapt the
value of the command line parameter `-c` to match it.

If the SIM card you are using does not need a PIN code, you can omit
`-pin [your PIN]`.

The output should be similar to:

```
use PC/SC mode
PC/SC reader name: HID Global OMNIKEY 6121 Smart Card Reader [OMNIKEY 6121 Smart Card Reader] 00 00
IMSI 999990000000112
listen on 0.0.0.0:4098, wait for a client
```

The server is now ready, running in PC/SC mode. Run the NR UE as explained
above.
