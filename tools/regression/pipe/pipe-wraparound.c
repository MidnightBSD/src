/*
Copyright (C) 2004 Michael J. Silbersack. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/*
 * $FreeBSD$
 * This program tests to make sure that wraparound writes and reads
 * are working, assuming that 16K socket buffers are used.  In order
 * to really stress the pipe code with this test, kernel modifications
 * nay be necessary.
 */

int main (void)
{
char buffer[32768], buffer2[32768];
int desc[2];
int buggy, error, i, successes, total;
struct stat status;
pid_t new_pid;

buggy = 0;
total = 0;

error = pipe(desc);

if (error)
	err(0, "Couldn't allocate fds\n");

buffer[0] = 'A';

for (i = 1; i < 32768; i++) {
	buffer[i] = buffer[i - 1] + 1;
	if (buffer[i] > 'Z')
		buffer[i] = 'A';
	}

new_pid = fork();

if (new_pid == 0) {
	error = write(desc[1], &buffer, 4096);
	total += error;
	error = write(desc[1], &buffer[total], 4096);
	total += error;
	error = write(desc[1], &buffer[total], 4000);
	total += error;
	printf("Wrote %d bytes, sleeping\n", total);
	usleep(1000000);
	error = write(desc[1], &buffer[total], 3000);
	total += error;
	error = write(desc[1], &buffer[total], 3000);
	total += error;
	printf("Wrote another 6000 bytes, %d total, done\n", total);
} else {
	usleep(500000);
	error = read(desc[0], &buffer2, 8192);
	total += error;
	printf("Read %d bytes, going back to sleep\n", error);
	usleep(1000000);
	error = read(desc[0], &buffer2[total], 16384);
	total += error;
	printf("Read %d bytes, done\n", error);

	for (i = 0; i < total; i++) {
		if (buffer[i] != buffer2[i]) {
			buggy = 1;
			printf("Location %d input: %hhx output: %hhx\n",
					i, buffer[i], buffer2[i]);
		}
	}

if (buggy)
	printf("FAILURE\n");
else
	printf("SUCCESS\n");
}


}
