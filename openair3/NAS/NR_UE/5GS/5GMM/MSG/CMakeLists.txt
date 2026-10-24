# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(fgs_5gmm_lib OBJECT
    fgmm_lib.c
    fgs_service_request.c
    fgmm_service_accept.c
    fgmm_service_reject.c
    fgmm_registration_reject.c
    RegistrationRequest.c
    RegistrationAccept.c
    fgmm_identity_request.c
    FGSIdentityResponse.c
    FGSAuthenticationResponse.c
    FGSNASSecurityModeComplete.c
    FGSNASSecurityModeReject.c
    RegistrationComplete.c
    FGSUplinkNasTransport.c
    FGSDeregistrationRequestUEOriginating.c
    fgmm_authentication_failure.c
    fgmm_authentication_reject.c
    fgmm_configuration_update_command.c
    fgmm_configuration_update_complete.c
)

target_link_libraries(fgs_5gmm_lib PUBLIC fgs_5gmm_ies_lib)

target_include_directories(fgs_5gmm_lib PRIVATE ../IES/ ../../)
