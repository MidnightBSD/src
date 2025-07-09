/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include "mport.h"
#include "mport_private.h"

#define PACKET_SIZE 64
#define PING_TIMEOUT 2
#define MAX_RETRIES 3

static unsigned short calculateChecksum(unsigned short *buffer, int length);
static long getCurrentTime(void);
long ping(char *hostname);

static unsigned short
calculateChecksum(unsigned short *buffer, int length)
{
	unsigned long sum = 0;
	for (; length > 1; length -= 2) {
		sum += *buffer++;
	}
	if (length == 1) {
		sum += *(unsigned char *)buffer;
	}
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	return (unsigned short)(~sum);
}

static long
getCurrentTime(void)
{
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (time.tv_sec * 1000 + time.tv_nsec / 1000); // Milliseconds
}

/**
 * @brief Ping a host to determine the round trip time
 * 
 * @param hostname IP address or hostname to ping
 * @return long milliseconds
 */
long
ping(char *hostname)
{

	struct sockaddr_in dest_addr;
	struct icmp icmphdr;
	char packet[PACKET_SIZE];
	long rtt = 1000;
	int try = 0;
	int rtts[MAX_RETRIES];

	int sockfd = socket(AF_INET, SOCK_RAW, 1); // Use 1 for ICMP (ICMPv4)
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = inet_addr(hostname);

	for (; try < MAX_RETRIES; try++) {
		memset(packet, 0, sizeof(packet));
		icmphdr.icmp_type = ICMP_ECHO;
		icmphdr.icmp_code = 0;
		icmphdr.icmp_id = getpid();
		icmphdr.icmp_seq = try;
		icmphdr.icmp_cksum = 0;
		icmphdr.icmp_cksum = calculateChecksum((unsigned short *)&icmphdr, sizeof(icmphdr));

		if (sendto(sockfd, &icmphdr, sizeof(icmphdr), 0, (struct sockaddr *)&dest_addr,
			sizeof(dest_addr)) <= 0) {
			perror("sendto");
			return -1;
		}

		long start_time = getCurrentTime();

		char recv_packet[PACKET_SIZE];
		socklen_t addr_len = sizeof(dest_addr);
		if (recvfrom(sockfd, recv_packet, sizeof(recv_packet), 0,
			(struct sockaddr *)&dest_addr, &addr_len) <= 0) {
			perror("recvfrom");
			return -1;
		}

		long end_time = getCurrentTime();

		struct icmp *icmp_reply = (struct icmp *)(recv_packet + 20); // Skip IP header
		if (icmp_reply->icmp_type == ICMP_ECHOREPLY) {
			rtt = end_time - start_time;
			printf("Received packet from %s, RTT = %ldms\n", hostname, rtt);
            		rtts[try] = rtt;
		} else {
			printf("Received an ICMP packet of type %d\n", icmp_reply->icmp_type);
            		rtts[try] = -1;
		}

        sleep(1);
	}

	close(sockfd);

	int sum = 0;
	int totalValid = 0;
	for (int i = 0; i < MAX_RETRIES; i++) {
	    if (rtts[i] != -1) {
			sum += rtts[i];
			totalValid++;
	    }
	}

	if (totalValid == 0) {
	    return -1;
	}
	return sum / totalValid; // average
}
