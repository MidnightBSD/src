/*
 * Copyright (c) 2006 Cisco Systems, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX4_DRIVER_H
#define MLX4_DRIVER_H

#include <linux/device.h>

struct mlx4_dev;

enum mlx4_dev_event {
	MLX4_DEV_EVENT_CATASTROPHIC_ERROR,
	MLX4_DEV_EVENT_PORT_UP,
	MLX4_DEV_EVENT_PORT_DOWN,
	MLX4_DEV_EVENT_PORT_REINIT,
};

enum mlx4_query_reply {
	MLX4_QUERY_NOT_MINE	= -1,
	MLX4_QUERY_MINE_NOPORT 	= 0
};

enum mlx4_prot {
	MLX4_PROT_IB,
	MLX4_PROT_EN,
};

enum mlx4_mcast_prot {
	MLX4_MCAST_PROT_IB = 0,
	MLX4_MCAST_PROT_EN = 1,
};

struct mlx4_interface {
	void *			(*add)	 (struct mlx4_dev *dev);
	void			(*remove)(struct mlx4_dev *dev, void *context);
	void			(*event) (struct mlx4_dev *dev, void *context,
					  enum mlx4_dev_event event, int port);
	void *  (*get_prot_dev) (struct mlx4_dev *dev, void *context, u8 port);
	enum mlx4_prot          protocol;

	enum mlx4_query_reply	(*query) (void *context, void *);
	struct list_head	list;
};

int mlx4_register_interface(struct mlx4_interface *intf);
void mlx4_unregister_interface(struct mlx4_interface *intf);
void *mlx4_get_prot_dev(struct mlx4_dev *dev, enum mlx4_prot proto, int port);

struct mlx4_dev *mlx4_query_interface(void *, int *port);
void mlx4_set_iboe_counter(struct mlx4_dev *dev, int index, u8 port);
int mlx4_get_iboe_counter(struct mlx4_dev *dev, u8 port);

#endif /* MLX4_DRIVER_H */
