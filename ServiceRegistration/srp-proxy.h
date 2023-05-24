/* srp-proxy.h
 *
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Service Registration Protocol common definitions.
 */

#ifndef __SRP_PROXY_H
#define __SRP_PROXY_H

typedef struct srp_proxy_listener_state srp_proxy_listener_state_t;
typedef struct srp_server_state srp_server_t;
typedef struct srpl_connection srpl_connection_t;

void srp_proxy_listener_cancel(srp_proxy_listener_state_t *NONNULL listener_state);
srp_proxy_listener_state_t *NULLABLE srp_proxy_listen(uint16_t *NULLABLE avoid_ports, int num_avoid_ports,
                                                      ready_callback_t NULLABLE ready,
                                                      srp_server_t *NONNULL server_state);
void srp_proxy_init(const char *NONNULL update_zone);
bool srp_evaluate(comm_t *NONNULL comm, srp_server_t *NULLABLE server_state,
                  srpl_connection_t *NULLABLE srpl_connection,
                  dns_message_t *NONNULL message, message_t *NONNULL raw_message);
bool srp_update_start(comm_t *NONNULL connection, srp_server_t *NULLABLE server_state,
                      srpl_connection_t *NULLABLE srpl_connection,
                      dns_message_t *NONNULL parsed_message, message_t *NONNULL raw_message,
                      dns_host_description_t *NONNULL new_host, service_instance_t *NONNULL instances,
                      service_t *NONNULL services, delete_t *NULLABLE removes, dns_name_t *NONNULL update_zone,
                      uint32_t lease_time, uint32_t key_lease_time, uint32_t serial_number, bool found_serial);
void srp_update_free_parts(service_instance_t *NULLABLE service_instances, service_instance_t *NULLABLE added_instances,
                           service_t *NULLABLE services, delete_t *NULLABLE removes,
                           dns_host_description_t *NULLABLE host_description);
void srp_update_free(update_t *NONNULL update);


// Provided
void dns_input(comm_t *NONNULL comm, message_t *NONNULL message, void *NULLABLE context);
void srp_mdns_flush(srp_server_t *NONNULL server_state);
bool srp_dns_evaluate(comm_t *NULLABLE connection, srp_server_t *NULLABLE server_state,
                      srpl_connection_t *NULLABLE srpl_connection, message_t *NONNULL message);
#endif // __SRP_PROXY_H

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
