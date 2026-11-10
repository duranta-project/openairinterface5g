# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(remote_sim_server
    uicc.c
    atr.c
    tpdu-apdu.c
    main.c
    net.c
    pl2303.c
    usim.c
    util.c
    log.c
)
