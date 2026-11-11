<!-- SPDX-License-Identifier: CC-BY-4.0 -->

## config module source files

```
                           +------------------------------------+
                           | OAI root/openairinterface5g/common |
                           +------------------------------------+
                                              |
                            +----------------------------------+
                            |              Config              |
                            |  config_load_configmodule.c .h   |
                            |        config_paramdesc.h        |
                            |       config_userapi.c .h        |
                            |         config_cmdline.c         |
                            +----------------------------------+
                               |           |                |
               +---------------+           |                +-------------+
               v                           v                              v
+----------------------------+   +- - - - - - - - - -+  +----------------------------------+
|         libconfig          |   :  <config source>  :  |    config_load_configmodule.o    |
|     config_libconfig.c     |   :                   :  |         config_userapi.o         |
|     config_libconfig.h     |   :                   :  |         config_cmdline.o         |
| config_libconfig_private.h |   :                   :  | To be linked with oai executable |
+----------------------------+   +- - - - - - - - - -+  +----------------------------------+
               |                           |
               v                           v
    libparams_libconfig.so     libparams_<config source>.so
```

## config module components

```
              +-------------------------------------+
              | OAI executable                      |
              |  /openair2/ENB_APP/enb_config.c     |
              |                                     |
              |  Config module interface            |
              |  macros/calls:                      |
              |    config_get                       |
              |    config_getlist                   |
              +-------------------------------------+
                                ^
                                | [R] <-->
                                v
+-------------------------------------------------------------+
|                          config_libconfig_get               |
| config module shared     config_libconfig_getlist           |
| library                                                     |
| libparams_libconfig.so   Libconfig calls:                   |
|                          config_setting_lookup_XXXX         |
|                          config_lookup_XXXX                 |
+-------------------------------------------------------------+
                                ^
                                | [B] <-->
                                v
              +-------------------------------------+               _______________
              | Libconfig.so shared library         |              (_______________)
              |                                     |   <======>   | Parameter file|
              |  config_setting_lookup_XXX          |              | P1= nnn       |
              |  config_lookup_XXX                  |              | ...           |
              +-------------------------------------+              | Pn= "xyz"     |
                                                                   | ...           |
                                                                   (_______________)

  [R]  Run time load        [B]  Build time link
  <==> Disk IO              <--> F() call
```

[Configuration module home](../config.md)
