/******************************************************************************
 * sndif.h
 *
 * Unified sound-device I/O interface for Xen guest OSes.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (C) 2013-2015 GlobalLogic Inc.
 */

#ifndef __XEN_PUBLIC_IO_XENSND_H__
#define __XEN_PUBLIC_IO_XENSND_H__

/*
 * Front->back notifications: When enqueuing a new request, sending a
 * notification can be made conditional on req_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Backends must set
 * req_event appropriately (e.g., using RING_FINAL_CHECK_FOR_REQUESTS()).
 *
 * Back->front notifications: When enqueuing a new response, sending a
 * notification can be made conditional on rsp_event (i.e., the generic
 * hold-off mechanism provided by the ring macros). Frontends must set
 * rsp_event appropriately (e.g., using RING_FINAL_CHECK_FOR_RESPONSES()).
 */

/*
 * Feature and Parameter Negotiation
 * =================================
 * The two halves of a Para-virtual sound card driver utilize nodes within the
 * XenStore to communicate capabilities and to negotiate operating parameters.
 * This section enumerates these nodes which reside in the respective front and
 * backend portions of the XenStore, following the XenBus convention.
 *
 * All data in the XenStore is stored as strings.  Nodes specifying numeric
 * values are encoded in decimal.  Integer value ranges listed below are
 * expressed as fixed sized integer types capable of storing the conversion
 * of a properly formated node string, without loss of information.
 *
 *****************************************************************************
 *                            Backend XenBus Nodes
 *****************************************************************************
 *
 *------------------------- Backend Device parameters -------------------------
 *
 * devid
 *      Values:         <uint32_t>
 *
 *      Index of the soundcard which will be created in the frontend. This
 *      index is zero based.
 *
 * devcnt
 *      Values:         <uint32_t>
 *
 *      PCM instances count that are created by the soundcard in the frontend.
 *
 *----------------------------- Streams settings ------------------------------
 *
 * Every virtualized device has own set of the sound streams. Each stream
 * parameter is with index "%u" and defined as 'stream%u_???'. Stream index is
 * zero based and should be continuous in range from 0 to 'streams_cnt' - 1.
 *
 * stream%u_channels
 *      Values:         <uint32_t>
 *
 *      The maximum amount of channels that can be supported by this stream.
 *      Should be from 1 to XENSND_MAX_CHANNELS_PER_STREAM.
 *
 * stream%u_type
 *      Values:         "p", "c", "b"
 *
 *      Stream type: "p" - playback stream, "c" - capture stream,
 *      "b" - both playback and capture stream.
 *
 * stream%u_bedev_p
 *      Values:         string
 *
 *      Name of the playback sound device which is mapped to this stream
 *      by the backend. Present if stream%u_type is "p" or "b"
 *
 * stream%u_bedev_c
 *      Values:         string
 *
 *      Name of the cappture sound device which is mapped to this stream
 *      by the backend. Present if stream%u_type is "c" or "b"
 *
 * stream%u_devid
 *      Values:         <uint32_t>
 *
 *      Index of the PCM instance which is created by the soundcard
 *      in the frontend.
 *
 *****************************************************************************
 *                            Frontend XenBus Nodes
 *****************************************************************************
 *
 *----------------------- Request Transport Parameters -----------------------
 *
 * event-channel
 *      Values:         <uint32_t>
 *
 *      The identifier of the Xen event channel used to signal activity
 *      in the ring buffer.
 *
 * ring-ref
 *      Values:         <uint32_t>
 *
 *      The Xen grant reference granting permission for the backend to map
 *      the sole page in a single page sized ring buffer.
 */

/*
 * STATE DIAGRAMS
 *
 *****************************************************************************
 *                                   Startup                                 *
 *****************************************************************************
 *
 * Tool stack creates front and back nodes with state XenbusStateInitialising.
 *
 * Front                                Back
 * =================================    =====================================
 * XenbusStateInitialising              XenbusStateInitialising
 *  o Query virtual device               o Query backend device identification
 *    properties.                          data.
 *  o Setup OS device instance.          o Open and validate backend device.
 *                                       o Publish backend features and
 *                                         transport parameters.
 *                                                      |
 *                                                      |
 *                                                      V
 *                                      XenbusStateInitWait
 *
 * o Query backend features and
 *   transport parameters.
 * o Allocate and initialize the
 *   request ring.
 * o Publish transport parameters
 *   that will be in effect during
 *   this connection.
 *              |
 *              |
 *              V
 * XenbusStateInitialised
 *
 *                                       o Query frontend transport parameters.
 *                                       o Connect to the request ring and
 *                                         event channel.
 *                                       o Publish backend device properties.
 *                                                      |
 *                                                      |
 *                                                      V
 *                                      XenbusStateConnected
 *
 *  o Query backend device properties.
 *  o Finalize OS virtual device
 *    instance.
 *              |
 *              |
 *              V
 * XenbusStateConnected
 *
 * Note: Drivers that do not support any optional features, or the negotiation
 *       of transport parameters, can skip certain states in the state machine:
 *
 *       o A frontend may transition to XenbusStateInitialised without
 *         waiting for the backend to enter XenbusStateInitWait.  In this
 *         case, default transport parameters are in effect and any
 *         transport parameters published by the frontend must contain
 *         their default values.
 *
 *       o A backend may transition to XenbusStateInitialised, bypassing
 *         XenbusStateInitWait, without waiting for the frontend to first
 *         enter the XenbusStateInitialised state.  In this case, default
 *         transport parameters are in effect and any transport parameters
 *         published by the backend must contain their default values.
 *
 *       Drivers that support optional features and/or transport parameter
 *       negotiation must tolerate these additional state transition paths.
 *       In general this means performing the work of any skipped state
 *       transition, if it has not already been performed, in addition to the
 *       work associated with entry into the current state.
 */

/*
 * PCM FORMATS
 *
 * XENSND_PCM_FORMAT_<format>[_<endian>]
 *
 * format: <S/U/F><bits> or <name>
 *     S - signed, U - unsigned, F - float
 *     bits - 8, 16, 24, 32
 *     name - MU_LAW, GSM, etc.
 *
 * endian: <LE/BE>, may be absent
 *     LE - Little endian, BE - Big endian
 */
#define XENSND_PCM_FORMAT_S8            0
#define XENSND_PCM_FORMAT_U8            1
#define XENSND_PCM_FORMAT_S16_LE        2
#define XENSND_PCM_FORMAT_S16_BE        3
#define XENSND_PCM_FORMAT_U16_LE        4
#define XENSND_PCM_FORMAT_U16_BE        5
#define XENSND_PCM_FORMAT_S24_LE        6
#define XENSND_PCM_FORMAT_S24_BE        7
#define XENSND_PCM_FORMAT_U24_LE        8
#define XENSND_PCM_FORMAT_U24_BE        9
#define XENSND_PCM_FORMAT_S32_LE        10
#define XENSND_PCM_FORMAT_S32_BE        11
#define XENSND_PCM_FORMAT_U32_LE        12
#define XENSND_PCM_FORMAT_U32_BE        13
#define XENSND_PCM_FORMAT_F32_LE        14 /* 4-byte float, IEEE-754 32-bit, */
#define XENSND_PCM_FORMAT_F32_BE        15 /* range -1.0 to 1.0              */
#define XENSND_PCM_FORMAT_F64_LE        16 /* 8-byte float, IEEE-754 64-bit, */
#define XENSND_PCM_FORMAT_F64_BE        17 /* range -1.0 to 1.0              */
#define XENSND_PCM_FORMAT_IEC958_SUBFRAME_LE 18
#define XENSND_PCM_FORMAT_IEC958_SUBFRAME_BE 19
#define XENSND_PCM_FORMAT_MU_LAW        20
#define XENSND_PCM_FORMAT_A_LAW         21
#define XENSND_PCM_FORMAT_IMA_ADPCM     22
#define XENSND_PCM_FORMAT_MPEG          23
#define XENSND_PCM_FORMAT_GSM           24
#define XENSND_PCM_FORMAT_SPECIAL       31 /* Any other unspecified format */

/*
 * REQUEST CODES.
 */
#define XENSND_OP_OPEN                  0
#define XENSND_OP_CLOSE                 1
#define XENSND_OP_READ                  2
#define XENSND_OP_WRITE                 3
#define XENSND_OP_SET_VOLUME            4
#define XENSND_OP_GET_VOLUME            5

/*
 * The maximum amount of shared pages which can be used in any request
 * from the frontend driver to the backend driver
 */
#define XENSND_MAX_PAGES_PER_REQUEST    10

/* The maximum amount of channels per virtualized stream */
#define XENSND_MAX_CHANNELS_PER_STREAM  128

/*
 * STATUS RETURN CODES.
 */
 /* Operation failed for some unspecified reason (e. g. -EIO). */
#define XENSND_RSP_ERROR                 (-1)
 /* Operation completed successfully. */
#define XENSND_RSP_OKAY                  0

/*
 * Description of the protocol between frontend and backend driver.
 *
 * The two halves of a Para-virtual sound driver communicates with
 * each to other using an shared page and event channel.
 * Shared page contains a ring with request/response packets.
 * All fields within the packet are always in little-endian byte order.
 * Almost all fields within the packet are unsigned except
 * the field 'status' in the responses packets which is signed.
 *
 *
 * All request packets have the same length (64 bytes)
 *
 * Request open - open an pcm stream for playback or capture:
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |                      id                       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       operation       |      stream_idx       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |      pcm_format       |      pcm_channels     |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       pcm_rate        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id - private guest value, echoed in resp
 * operation - XENSND_OP_OPEN
 * stream_idx - index of the stream (from 0 to 'streams_cnt' - 1.
 *   'streams_cnt' is read from the XenStore)
 * pcm_format - XENSND_PCM_FORMAT_???
 * pcm_channels - channels count in stream
 * pcm_rate - stream data rate
 *
 *
 * Request close - close an opened pcm stream:
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |                      id                       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       operation       |       stream_idx      |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id - private guest value, echoed in resp
 * operation - XENSND_OP_CLOSE
 * stream_idx - index of the stream (from 0 to 'streams_cnt' - 1.
 *   'streams_cnt' is read from the XenStore)
 *
 *
 * Request read/write - used for read (for capture) or write (for playback):
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |                      id                       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       operation       |       stream_idx      |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         length        |         gref0         |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         gref1         |         gref2         |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |          gref9        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id - private guest value, echoed in resp
 * operation - XENSND_OP_READ or XENSND_OP_WRITE
 * stream_idx - index of the stream (from 0 to 'streams_cnt' - 1.
 *   'streams_cnt' is read from the XenStore)
 * length - read or write data length
 * gref0 - gref9 - references to a grant entries for used pages in read/write
 * request.
 *
 *
 * Request set volume - set/get channels volume in stream:
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |                      id                       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       operation       |       stream_idx      |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         gref          |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id - private guest value, echoed in resp
 * operation - XENSND_OP_SET_VOLUME or XENSND_OP_GET_VOLUME
 * stream_idx - index of the stream (from 0 to 'streams_cnt' - 1.
 *   'streams_cnt' is read from the XenStore)
 * gref - references to a grant entry for page with the volume values
 *
 *
 * Shared page for set/get volume:
 *
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |        vol_ch0        |        vol_ch1        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |        vol_ch2        |        vol_ch3        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       vol_ch126       |       vol_ch127       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * vol_ch0 - vol_ch127 - volume for the channel from 0 to
 *   XENSND_MAX_CHANNELS_PER_STREAM
 * Please, note that only first 'stream%u_channels' are used in this command,
 *   where 'stream%u_channels' is read from the XenStore (channels count for
 *   stream with index '%u' which equals to 'stream_idx')
 *
 *
 * All response packets have the same length (64 bytes)
 *
 * Response for all requests:
 *     0    1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |                      id                       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       operation       |         status        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       stream_idx      |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * +/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/+
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |       reserved        |       reserved        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id - copied from request
 * stream_idx - copied from request
 * operation - XENSND_OP_??? - copied from request
 * status - XENSND_RSP_???
 */

struct xensnd_request {
    uint8_t raw[64];
};

struct xensnd_response {
    uint8_t raw[64];
};

DEFINE_RING_TYPES(xensnd, struct xensnd_request, struct xensnd_response);

#endif /* __XEN_PUBLIC_IO_XENSND_H__ */
