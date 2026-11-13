# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(remote_sim OBJECT
            remote_sim.c
            remote_sim_net.c
)

target_link_libraries(remote_sim PRIVATE log_headers)

add_subdirectory(server)
