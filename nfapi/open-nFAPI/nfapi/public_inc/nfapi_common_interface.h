/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NFAPI_COMMON_INTERFACE_H
#define NFAPI_COMMON_INTERFACE_H
#include "nfapi/open-nFAPI/nfapi/public_inc/fapi_nr_ue_constants.h"

typedef struct {
  uint16_t tag;
  uint16_t length;
} nfapi_tl_t;

/*! Configuration options for the p7 pack unpack functions
 *
 */

typedef struct nfapi_p7_codec_config {
  /*! Optional call back to allow the user to define the memory allocator.
   *  \param size The size of the memory to allocate
   *  \return a pointer to a valid memory block or 0 if it has failed.
   *
   * If not set the nfapi unpack functions will use malloc
   */
  void* (*allocate)(size_t size);

  /*! Optional call back to allow the user to define the memory deallocator.
   *  \param ptr A poiner to a memory block allocated by the allocate callback
   *
   *	If not set the client should use free
   */
  void (*deallocate)(void* ptr);

  /*! Optional call back function to handle unpacking vendor extension tlv.
   *  \param tl A pointer to a decoded tag length structure
   *  \param ppReadPackedMsg A handle to the read buffer.
   *  \param end The end of the read buffer
   *  \param ve A handle to a vendor extention structure that the call back should allocate if the structure can be decoded
   *  \param config A pointer to the p7 codec configuration
   *  \return return 0 if packed successfully, -1 if failed.
   *
   *  If not set the tlv will be skipped
   *
   *  Client should use the help methods in nfapi.h to decode the vendor extention.
   *
   *  \todo Add code example
   */
  int (*unpack_vendor_extension_tlv)(nfapi_tl_t* tl,
                                     uint8_t** ppReadPackedMsg,
                                     uint8_t* end,
                                     void** ve,
                                     struct nfapi_p7_codec_config* config);

  /*! Optional call back function to handle packing vendor extension tlv.
   *  \param ve A pointer to a vendor extention structure.
   *  \param ppWritePackedMsg A handle to the write buffer
   *  \param end The end of the write buffer. The callee should make sure not to write beyond the end
   *  \param config A pointer to the p7 codec configuration
   *  \return return 0 if packed successfully, -1 if failed.
   *
   *  If not set the the tlv will be skipped
   *
   *  Client should use the help methods in nfapi.h to encode the vendor extention
   *
   *  \todo Add code example
   */
  int (*pack_vendor_extension_tlv)(void* ve, uint8_t** ppWritePackedMsg, uint8_t* end, struct nfapi_p7_codec_config* config);

  /*! Optional call back function to handle unpacking vendor extension messages.
   *  \param header A pointer to a decode P7 message header for the vendor extention message
   *  \param ppReadPackedMsg A handle to the encoded data buffer
   *  \param end A pointer to the end of the encoded data buffer
   *  \param config  A pointer to the p7 codec configuration
   *  \return 0 if unpacked successfully, -1 if failed
   *
   *  If not set the message will be ignored
   *
   *  If the message if is unknown the function should return -1
   */
  int (*unpack_p7_vendor_extension)(void* header, uint8_t** ppReadPackedMsg, uint8_t* end, struct nfapi_p7_codec_config* config);

  /*! Optional call back function to handle packing vendor extension messages.
   *  \param header A poiner to a P7 message structure for the venfor extention message
   *  \param ppWritePackedmsg A handle to the buffer to write the encoded message into
   *  \param end A pointer to the end of the buffer
   *  \param cofig A pointer to the p7 codec configuration
   *  \return 0 if packed successfully, -1 if failed
   *
   * If not set the the message will be ingored
   *
   *  If the message if is unknown the function should return -1
   */
  int (*pack_p7_vendor_extension)(void* header, uint8_t** ppWritePackedmsg, uint8_t* end, struct nfapi_p7_codec_config* config);

  /*! Optional user data that will be passed back with callbacks
   */
  void* user_data;

} nfapi_p7_codec_config_t;

/*! Configuration options for the p4 & p5 pack unpack functions
 *
 */
typedef struct nfapi_p4_p5_codec_config {
  /*! Optional call back to allow the user to define the memory allocator.
   *  \param size The size of the memory to allocate
   *  \return a pointer to a valid memory block or 0 if it has failed.
   *
   *  If not set the nfapi unpack functions will use malloc
   */
  void* (*allocate)(size_t size);

  /*! Optional call back to allow the user to define the memory deallocator.
   *  \param ptr A poiner to a memory block allocated by the allocate callback
   *
   *  If not set free will be used
   */
  void (*deallocate)(void* ptr);

  /*! Optional call back function to handle unpacking vendor extension tlv.
   *  \param tl A pointer to a decoded tag length structure
   *  \param ppReadPackedMsg A handle to the data buffer to decode
   *  \param end A pointer to the end of the buffer
   *  \param ve A handle to a vendor extention structure that will be allocated by this callback
   *  \param config A pointer to the P4/P5 codec configuration
   *  \return 0 if unpacked successfully, -1 if failed
   *
   *  If not set the tlv will be skipped
   */
  int (*unpack_vendor_extension_tlv)(nfapi_tl_t* tl,
                                     uint8_t** ppReadPackedMsg,
                                     uint8_t* end,
                                     void** ve,
                                     struct nfapi_p4_p5_codec_config* config);

  /*! Optional call back function to handle packing vendor extension tlv.
   *  \param ve
   *  \param ppWritePackedMsg A handle to the data buffer pack the tlv into
   *  \param end A pointer to the end of the buffer
   *  \param config A pointer to the P4/P5 codec configuration
   *  \return 0 if packed successfully, -1 if failed
   *
   *  If not set the the tlv will be skipped
   */
  int (*pack_vendor_extension_tlv)(void* ve, uint8_t** ppWritePackedMsg, uint8_t* end, struct nfapi_p4_p5_codec_config* config);

  /*! Optional call back function to handle unpacking vendor extension messages.
   *  \param header A pointer to a decode P4/P5 message header
   *  \param ppReadPackgedMsg A handle to the data buffer to decode
   *  \param end A pointer to the end of the buffer
   *  \param config A pointer to the P4/P5 codec configuration
   *  \return 0 if packed successfully, -1 if failed
   *
   * If not set the message will be ignored
   */
  int (*unpack_p4_p5_vendor_extension)(void* header,
                                       uint8_t** ppReadPackedMsg,
                                       uint8_t* end,
                                       struct nfapi_p4_p5_codec_config* config);

  /*! Optional call back function to handle packing vendor extension messages.
   *  \param header A pointer to the P4/P5 message header to be encoded
   *  \param ppWritePackedMsg A handle to the data buffer pack the message into
   *  \param end A pointer to the end of the buffer
   *  \param config A pointer to the P4/P5 codec configuration
   *  \return 0 if packed successfully, -1 if failed
   *
   *  If not set the the message will be ingored
   */
  int (*pack_p4_p5_vendor_extension)(void* header,
                                     uint8_t** ppwritepackedmsg,
                                     uint8_t* end,
                                     struct nfapi_p4_p5_codec_config* config);

  /*! Optional user data that will be passed back with callbacks
   */
  void* user_data;

} nfapi_p4_p5_codec_config_t;

#endif // NFAPI_COMMON_INTERFACE_H
