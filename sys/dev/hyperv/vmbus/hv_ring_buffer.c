/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include "hv_vmbus_priv.h"

/* Amount of space to write to */
#define	HV_BYTES_AVAIL_TO_WRITE(r, w, z) ((w) >= (r))? \
				((z) - ((w) - (r))):((r) - (w))

/**
 * @brief Get number of bytes available to read and to write to
 * for the specified ring buffer
 */
static inline void
get_ring_buffer_avail_bytes(
	    hv_vmbus_ring_buffer_info*	rbi,
	    uint32_t*			read,
	    uint32_t*			write)
{
	uint32_t read_loc, write_loc;

	/*
	 * Capture the read/write indices before they changed
	 */
	read_loc = rbi->ring_buffer->read_index;
	write_loc = rbi->ring_buffer->write_index;

	*write = HV_BYTES_AVAIL_TO_WRITE(
		read_loc, write_loc, rbi->ring_data_size);
	*read = rbi->ring_data_size - *write;
}

/**
 * @brief Get the next write location for the specified ring buffer
 */
static inline uint32_t
get_next_write_location(hv_vmbus_ring_buffer_info* ring_info) 
{
	uint32_t next = ring_info->ring_buffer->write_index;
	return (next);
}

/**
 * @brief Set the next write location for the specified ring buffer
 */
static inline void
set_next_write_location(
	hv_vmbus_ring_buffer_info*	ring_info,
	uint32_t			next_write_location)
{
	ring_info->ring_buffer->write_index = next_write_location;
}

/**
 * @brief Get the next read location for the specified ring buffer
 */
static inline uint32_t
get_next_read_location(hv_vmbus_ring_buffer_info* ring_info) 
{
	uint32_t next = ring_info->ring_buffer->read_index;
	return (next);
}

/**
 * @brief Get the next read location + offset for the specified ring buffer.
 * This allows the caller to skip.
 */
static inline uint32_t
get_next_read_location_with_offset(
	hv_vmbus_ring_buffer_info*	ring_info,
	uint32_t			offset)
{
	uint32_t next = ring_info->ring_buffer->read_index;
	next += offset;
	next %= ring_info->ring_data_size;
	return (next);
}

/**
 * @brief Set the next read location for the specified ring buffer
 */
static inline void
set_next_read_location(
	hv_vmbus_ring_buffer_info*	ring_info,
	uint32_t			next_read_location)
{
	ring_info->ring_buffer->read_index = next_read_location;
}

/**
 * @brief Get the start of the ring buffer
 */
static inline void *
get_ring_buffer(hv_vmbus_ring_buffer_info* ring_info) 
{
	return (void *) ring_info->ring_buffer->buffer;
}

/**
 * @brief Get the size of the ring buffer.
 */
static inline uint32_t
get_ring_buffer_size(hv_vmbus_ring_buffer_info* ring_info) 
{
	return ring_info->ring_data_size;
}

/**
 * Get the read and write indices as uint64_t of the specified ring buffer.
 */
static inline uint64_t
get_ring_buffer_indices(hv_vmbus_ring_buffer_info* ring_info) 
{
	return (uint64_t) ring_info->ring_buffer->write_index << 32;
}

static uint32_t	copy_to_ring_buffer(
			hv_vmbus_ring_buffer_info*	ring_info,
			uint32_t			start_write_offset,
			char*				src,
			uint32_t			src_len);

static uint32_t copy_from_ring_buffer(
			hv_vmbus_ring_buffer_info*	ring_info,
			char*				dest,
			uint32_t			dest_len,
			uint32_t			start_read_offset);


/**
 * @brief Get the interrupt mask for the specified ring buffer.
 */
uint32_t
hv_vmbus_get_ring_buffer_interrupt_mask(hv_vmbus_ring_buffer_info *rbi) 
{
	return rbi->ring_buffer->interrupt_mask;
}

/**
 * @brief Initialize the ring buffer.
 */
int
hv_vmbus_ring_buffer_init(
	hv_vmbus_ring_buffer_info*	ring_info,
	void*				buffer,
	uint32_t			buffer_len)
{
	memset(ring_info, 0, sizeof(hv_vmbus_ring_buffer_info));

	ring_info->ring_buffer = (hv_vmbus_ring_buffer*) buffer;
	ring_info->ring_buffer->read_index =
	    ring_info->ring_buffer->write_index = 0;

	ring_info->ring_size = buffer_len;
	ring_info->ring_data_size = buffer_len - sizeof(hv_vmbus_ring_buffer);

	mtx_init(&ring_info->ring_lock, "vmbus ring buffer", NULL, MTX_SPIN);

	return (0);
}

/**
 * @brief Cleanup the ring buffer.
 */
void hv_ring_buffer_cleanup(hv_vmbus_ring_buffer_info* ring_info) 
{
	mtx_destroy(&ring_info->ring_lock);
}

/**
 * @brief Write to the ring buffer.
 */
int
hv_ring_buffer_write(
	hv_vmbus_ring_buffer_info*	out_ring_info,
	hv_vmbus_sg_buffer_list		sg_buffers[],
	uint32_t			sg_buffer_count)
{
	int i = 0;
	uint32_t byte_avail_to_write;
	uint32_t byte_avail_to_read;
	uint32_t total_bytes_to_write = 0;

	volatile uint32_t next_write_location;
	uint64_t prev_indices = 0;

	for (i = 0; i < sg_buffer_count; i++) {
	    total_bytes_to_write += sg_buffers[i].length;
	}

	total_bytes_to_write += sizeof(uint64_t);

	mtx_lock_spin(&out_ring_info->ring_lock);

	get_ring_buffer_avail_bytes(out_ring_info, &byte_avail_to_read,
	    &byte_avail_to_write);

	/*
	 * If there is only room for the packet, assume it is full.
	 * Otherwise, the next time around, we think the ring buffer
	 * is empty since the read index == write index
	 */

	if (byte_avail_to_write <= total_bytes_to_write) {

	    mtx_unlock_spin(&out_ring_info->ring_lock);
	    return (EAGAIN);
	}

	/*
	 * Write to the ring buffer
	 */
	next_write_location = get_next_write_location(out_ring_info);

	for (i = 0; i < sg_buffer_count; i++) {
	    next_write_location = copy_to_ring_buffer(out_ring_info,
		next_write_location, (char *) sg_buffers[i].data,
		sg_buffers[i].length);
	}

	/*
	 * Set previous packet start
	 */
	prev_indices = get_ring_buffer_indices(out_ring_info);

	next_write_location = copy_to_ring_buffer(
		out_ring_info, next_write_location,
		(char *) &prev_indices, sizeof(uint64_t));

	/*
	 * Make sure we flush all writes before updating the writeIndex
	 */
	wmb();

	/*
	 * Now, update the write location
	 */
	set_next_write_location(out_ring_info, next_write_location);

	mtx_unlock_spin(&out_ring_info->ring_lock);

	return (0);
}

/**
 * @brief Read without advancing the read index.
 */
int
hv_ring_buffer_peek(
	hv_vmbus_ring_buffer_info*	in_ring_info,
	void*				buffer,
	uint32_t			buffer_len)
{
	uint32_t bytesAvailToWrite;
	uint32_t bytesAvailToRead;
	uint32_t nextReadLocation = 0;

	mtx_lock_spin(&in_ring_info->ring_lock);

	get_ring_buffer_avail_bytes(in_ring_info, &bytesAvailToRead,
		&bytesAvailToWrite);

	/*
	 * Make sure there is something to read
	 */
	if (bytesAvailToRead < buffer_len) {
	    mtx_unlock_spin(&in_ring_info->ring_lock);
	    return (EAGAIN);
	}

	/*
	 * Convert to byte offset
	 */
	nextReadLocation = get_next_read_location(in_ring_info);

	nextReadLocation = copy_from_ring_buffer(
		in_ring_info, (char *)buffer, buffer_len, nextReadLocation);

	mtx_unlock_spin(&in_ring_info->ring_lock);

	return (0);
}

/**
 * @brief Read and advance the read index.
 */
int
hv_ring_buffer_read(
	hv_vmbus_ring_buffer_info*	in_ring_info,
	void*				buffer,
	uint32_t			buffer_len,
	uint32_t			offset)
{
	uint32_t bytes_avail_to_write;
	uint32_t bytes_avail_to_read;
	uint32_t next_read_location = 0;
	uint64_t prev_indices = 0;

	if (buffer_len <= 0)
	    return (EINVAL);

	mtx_lock_spin(&in_ring_info->ring_lock);

	get_ring_buffer_avail_bytes(
	    in_ring_info, &bytes_avail_to_read,
	    &bytes_avail_to_write);

	/*
	 * Make sure there is something to read
	 */
	if (bytes_avail_to_read < buffer_len) {
	    mtx_unlock_spin(&in_ring_info->ring_lock);
	    return (EAGAIN);
	}

	next_read_location = get_next_read_location_with_offset(
	    in_ring_info,
	    offset);

	next_read_location = copy_from_ring_buffer(
	    in_ring_info,
	    (char *) buffer,
	    buffer_len,
	    next_read_location);

	next_read_location = copy_from_ring_buffer(
	    in_ring_info,
	    (char *) &prev_indices,
	    sizeof(uint64_t),
	    next_read_location);

	/*
	 * Make sure all reads are done before we update the read index since
	 * the writer may start writing to the read area once the read index
	 * is updated.
	 */
	wmb();

	/*
	 * Update the read index
	 */
	set_next_read_location(in_ring_info, next_read_location);

	mtx_unlock_spin(&in_ring_info->ring_lock);

	return (0);
}

/**
 * @brief Helper routine to copy from source to ring buffer.
 *
 * Assume there is enough room. Handles wrap-around in dest case only!
 */
uint32_t
copy_to_ring_buffer(
	hv_vmbus_ring_buffer_info*	ring_info,
	uint32_t 			start_write_offset,
	char*				src,
	uint32_t			src_len)
{
	char *ring_buffer = get_ring_buffer(ring_info);
	uint32_t ring_buffer_size = get_ring_buffer_size(ring_info);
	uint32_t fragLen;

	if (src_len > ring_buffer_size - start_write_offset)  {
	    /* wrap-around detected! */
	    fragLen = ring_buffer_size - start_write_offset;
	    memcpy(ring_buffer + start_write_offset, src, fragLen);
	    memcpy(ring_buffer, src + fragLen, src_len - fragLen);
	} else {
	    memcpy(ring_buffer + start_write_offset, src, src_len);
	}

	start_write_offset += src_len;
	start_write_offset %= ring_buffer_size;

	return (start_write_offset);
}

/**
 * @brief Helper routine to copy to source from ring buffer.
 *
 * Assume there is enough room. Handles wrap-around in src case only!
 */
uint32_t
copy_from_ring_buffer(
	hv_vmbus_ring_buffer_info*	ring_info,
	char*				dest,
	uint32_t			dest_len,
	uint32_t			start_read_offset)
{
	uint32_t fragLen;
	char *ring_buffer = get_ring_buffer(ring_info);
	uint32_t ring_buffer_size = get_ring_buffer_size(ring_info);

	if (dest_len > ring_buffer_size - start_read_offset) {
	    /*  wrap-around detected at the src */
	    fragLen = ring_buffer_size - start_read_offset;
	    memcpy(dest, ring_buffer + start_read_offset, fragLen);
	    memcpy(dest + fragLen, ring_buffer, dest_len - fragLen);
	} else {
	    memcpy(dest, ring_buffer + start_read_offset, dest_len);
	}

	start_read_offset += dest_len;
	start_read_offset %= ring_buffer_size;

	return (start_read_offset);
}

