/*
 *  Unified sound-device I/O interface for Xen guest OSes
 *  Copyright (c) 2016, Oleksandr Andrushchenko
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __XEN_PUBLIC_IO_XENSND_LINUX_H__
#define __XEN_PUBLIC_IO_XENSND_LINUX_H__

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>
#include <xen/interface/grant_table.h>
#if 0
#include <xen/include/public/io/sndif.h>
#else
#warning "TODO: properly include base protocol header"
#include "sndif.h"
#endif

struct xensnd_open_req {
	uint8_t pcm_format;
	uint8_t pcm_channels;
	uint8_t pcm_rate;
} __attribute__((packed));

struct xensnd_req {
	union {
		struct xensnd_request raw;
		struct {
			uint8_t id;
			uint8_t operation;
			uint8_t stream_idx;
			union {
				struct xensnd_open_req open;
			} op;
		} data;
	} u;
};

struct xensnd_resp {
	union {
		struct xensnd_response raw;
		struct {
			uint8_t id;
			uint8_t operation;
			uint8_t stream_idx;
			uint8_t status;
		} data;
	} u;
};

DEFINE_RING_TYPES(xen_sndif, struct xensnd_request,
		struct xensnd_response);

#endif /* __XEN_PUBLIC_IO_XENSND_LINUX_H__ */
