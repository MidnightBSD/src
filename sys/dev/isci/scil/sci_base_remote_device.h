/*-
 * SPDX-License-Identifier: BSD-2-Clause OR GPL-2.0
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _SCI_BASE_REMOTE_DEVICE_H_
#define _SCI_BASE_REMOTE_DEVICE_H_

/**
 * @file
 *
 * @brief This file contains all of the structures, constants, and methods
 *        common to all remote device object definitions.
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <dev/isci/scil/sci_base_state_machine.h>
#include <dev/isci/scil/sci_base_logger.h>
#include <dev/isci/scil/sci_base_state_machine_logger.h>

struct SCI_BASE_REQUEST;

/**
 * @enum SCI_BASE_REMOTE_DEVICE_STATES
 *
 * @brief This enumeration depicts all the states for the common remote device
 *        state machine.
 */
typedef enum _SCI_BASE_REMOTE_DEVICE_STATES
{
   /**
    * Simply the initial state for the base remote device state machine.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_INITIAL,

   /**
    * This state indicates that the remote device has successfully been
    * stopped.  In this state no new IO operations are permitted.
    * This state is entered from the INITIAL state.
    * This state is entered from the STOPPING state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_STOPPED,

   /**
    * This state indicates the remote device is in the process of
    * becoming ready (i.e. starting).  In this state no new IO operations
    * are permitted.
    * This state is entered from the STOPPED state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_STARTING,

   /**
    * This state indicates the remote device is now ready.  Thus, the user
    * is able to perform IO operations on the remote device.
    * This state is entered from the STARTING state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_READY,

   /**
    * This state indicates that the remote device is in the process of
    * stopping.  In this state no new IO operations are permitted, but
    * existing IO operations are allowed to complete.
    * This state is entered from the READY state.
    * This state is entered from the FAILED state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_STOPPING,

   /**
    * This state indicates that the remote device has failed.
    * In this state no new IO operations are permitted.
    * This state is entered from the INITIALIZING state.
    * This state is entered from the READY state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_FAILED,

   /**
    * This state indicates the device is being reset.
    * In this state no new IO operations are permitted.
    * This state is entered from the READY state.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_RESETTING,

#if !defined(DISABLE_WIDE_PORTED_TARGETS)
   /**
    * This state indicates the device is in the middle of updating
    * its port width. All the IOs sent to this device will be Quiesced.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_UPDATING_PORT_WIDTH,
#endif

   /**
    * Simply the final state for the base remote device state machine.
    */
   SCI_BASE_REMOTE_DEVICE_STATE_FINAL,

   SCI_BASE_REMOTE_DEVICE_MAX_STATES

} SCI_BASE_REMOTE_DEVICE_STATES;

/**
 * @struct SCI_BASE_REMOTE_DEVICE
 *
 * @brief The base remote device object abstracts the fields common to all
 *        SCI remote device objects.
 */
typedef struct SCI_BASE_REMOTE_DEVICE
{
   /**
    * The field specifies that the parent object for the base remote
    * device is the base object itself.
    */
   SCI_BASE_OBJECT_T parent;

   /**
    * This field contains the information for the base remote device state
    * machine.
    */
   SCI_BASE_STATE_MACHINE_T state_machine;

   #ifdef SCI_LOGGING
   /**
    * This field contains the state machine observer for the state machine.
    */
   SCI_BASE_STATE_MACHINE_LOGGER_T state_machine_logger;
   #endif // SCI_LOGGING

} SCI_BASE_REMOTE_DEVICE_T;


typedef SCI_STATUS (*SCI_BASE_REMOTE_DEVICE_HANDLER_T)(
   SCI_BASE_REMOTE_DEVICE_T *
);

typedef SCI_STATUS (*SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T)(
   SCI_BASE_REMOTE_DEVICE_T *,
   struct SCI_BASE_REQUEST *
);

typedef SCI_STATUS (*SCI_BASE_REMOTE_DEVICE_HIGH_PRIORITY_REQUEST_COMPLETE_HANDLER_T)(
   SCI_BASE_REMOTE_DEVICE_T *,
   struct SCI_BASE_REQUEST *,
   void *,
   SCI_IO_STATUS
);

/**
 * @struct SCI_BASE_CONTROLLER_STATE_HANDLER
 *
 * @brief This structure contains all of the state handler methods common to
 *        base remote device state machines.  Handler methods provide the ability
 *        to change the behavior for user requests or transitions depending
 *        on the state the machine is in.
 */
typedef struct SCI_BASE_REMOTE_DEVICE_STATE_HANDLER
{
   /**
    * The start_handler specifies the method invoked when a user attempts to
    * start a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T start_handler;

   /**
    * The stop_handler specifies the method invoked when a user attempts to
    * stop a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T stop_handler;

   /**
    * The fail_handler specifies the method invoked when a remote device
    * failure has occurred.  A failure may be due to an inability to
    * initialize/configure the device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T fail_handler;

   /**
    * The destruct_handler specifies the method invoked when attempting to
    * destruct a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T destruct_handler;

   /**
    * The reset handler specifies the method invloked when requesting to reset a
    * remote device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T reset_handler;

   /**
    * The reset complete handler specifies the method invloked when reporting
    * that a reset has completed to the remote device.
    */
   SCI_BASE_REMOTE_DEVICE_HANDLER_T reset_complete_handler;

   /**
    * The start_io_handler specifies the method invoked when a user
    * attempts to start an IO request for a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T start_io_handler;

   /**
    * The complete_io_handler specifies the method invoked when a user
    * attempts to complete an IO request for a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T complete_io_handler;

   /**
    * The continue_io_handler specifies the method invoked when a user
    * attempts to continue an IO request for a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T continue_io_handler;

   /**
    * The start_task_handler specifies the method invoked when a user
    * attempts to start a task management request for a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T start_task_handler;

   /**
    * The complete_task_handler specifies the method invoked when a user
    * attempts to complete a task management request for a remote device.
    */
   SCI_BASE_REMOTE_DEVICE_REQUEST_HANDLER_T complete_task_handler;

} SCI_BASE_REMOTE_DEVICE_STATE_HANDLER_T;

/**
 * @brief Construct the base remote device
 *
 * @param[in] this_remote_device This parameter specifies the base remote
 *            device to be constructed.
 * @param[in] logger This parameter specifies the logger associated with
 *            this base remote device object.
 * @param[in] state_table This parameter specifies the table of state
 *            definitions to be utilized for the remote device state machine.
 *
 * @return none
 */
void sci_base_remote_device_construct(
   SCI_BASE_REMOTE_DEVICE_T * this_device,
   SCI_BASE_LOGGER_T        * logger,
   SCI_BASE_STATE_T         * state_table
);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _SCI_BASE_REMOTE_DEVICE_H_
