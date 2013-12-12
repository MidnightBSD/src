/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2007-2012 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_HELPER_H
#define H_HELPER_H

#define kmDNSHelperServiceName "com.apple.mDNSResponderHelper"

enum mDNSPreferencesSetNameKey
{
    kmDNSComputerName = 1,
    kmDNSLocalHostName
};

enum mDNSUpDown
{
    kmDNSUp = 1,
    kmDNSDown
};

enum mDNSAutoTunnelSetKeysReplaceDelete
{
    kmDNSAutoTunnelSetKeysReplace = 1,
    kmDNSAutoTunnelSetKeysDelete
};

// helper parses the system keychain and returns the information to mDNSResponder.
// It returns four attributes. Attributes are defined after how they show up in
// keychain access utility (the actual attribute name to retrieve these are different).
enum mDNSKeyChainAttributes
{
    kmDNSKcWhere,   // Where
    kmDNSKcAccount, // Account
    kmDNSKcKey,     // Key
    kmDNSKcName     // Name
};

#define ERROR(x, y) x,
enum mDNSHelperErrors
{
    mDNSHelperErrorBase = 2300,
    #include "helper-error.h"
    mDNSHelperErrorEnd
};
#undef ERROR

#include "mDNSEmbeddedAPI.h"
#include "helpermsg-types.h"

extern const char *mDNSHelperError(int errornum);

extern mStatus mDNSHelperInit(void);
extern void mDNSRequestBPF(void);
extern int  mDNSPowerRequest(int key, int interval);
extern int  mDNSSetLocalAddressCacheEntry(int ifindex, int family, const v6addr_t ip, const ethaddr_t eth);
extern void mDNSNotify(const char *title, const char *msg);     // Both strings are UTF-8 text
extern void mDNSPreferencesSetName(int key, domainlabel *old, domainlabel *new);
extern int  mDNSKeychainGetSecrets(CFArrayRef *secrets);
extern void mDNSConfigureServer(int updown, const char *const prefix, const domainname *const fqdn);
extern int  mDNSAutoTunnelSetKeys(int replacedelete, v6addr_t local_inner,
                                  v6addr_t local_outer, short local_port, v6addr_t remote_inner,
                                  v6addr_t remote_outer, short remote_port, const char *const prefix, const domainname *const fqdn);
extern void mDNSSendWakeupPacket(unsigned ifid, char *eth_addr, char *ip_addr, int iteration);
extern void mDNSPacketFilterControl(uint32_t command, char * ifname, uint32_t count, pfArray_t portArray, pfArray_t protocolArray);
extern void mDNSSendKeepalive(v6addr_t sadd, v6addr_t dadd, uint16_t lport, uint16_t rport, unsigned seq, unsigned ack, uint16_t win);
extern int  mDNSRetrieveTCPInfo(int family, v6addr_t laddr, uint16_t lport, v6addr_t raddr, uint16_t rport, uint32_t *seq, uint32_t *ack, uint16_t *win, int32_t *intfid);
extern void mDNSGetRemoteMAC(mDNS *const m, int family, v6addr_t raddr);
extern void mDNSStoreSPSMACAddress(int family, v6addr_t spsaddr, char *ifname);

#endif /* H_HELPER_H */
