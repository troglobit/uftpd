/* TFTP Engine
 *
 * Copyright (c) 2014  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <arpa/inet.h>
#include <arpa/tftp.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

int tftp_session(int sd)
{
	static FILE *fp = NULL;
	int16_t op;
	static uint16_t block = 0;
	char buf[sizeof(struct tftphdr) + SEGSIZE];
	size_t i, len;
	struct sockaddr_in addr;
	socklen_t addr_len;
	struct tftphdr *th = (struct tftphdr *)buf;
	size_t hdrsz = th->th_data - buf;

	if (chdir("/srv/ftp"))
		fprintf(stderr, "Failed changing to server root directory: %m\n");

	memset(buf, 0, sizeof(buf));
	len = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addr_len);
	for (i = 0; i < len; i++)
		fprintf(stderr, "%02X ", buf[i]);
	fprintf(stderr, "\n");

	op     = ntohs(th->th_opcode);
	block  = ntohs(th->th_block);

	if (op == RRQ) {
		fprintf(stderr, "tftp RRQ %s from %s:%d\n", th->th_stuff, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
		fp = fopen(th->th_stuff, "r");
		if (!fp) {
			char *errstr = strerror(errno);

			fprintf(stderr, "Failed opening %s ... %s\n", th->th_stuff, errstr);
			memset(buf, 0, sizeof(buf));
			th->th_opcode = htons(ERROR);
			th->th_code   = htons(ENOTFOUND);
			len = strlen(errstr) + 1;
			strncpy(th->th_data, errstr, len);
			sendto(sd, buf, hdrsz + len, 0, (struct sockaddr *)&addr, sizeof(addr));
			return 1;
		}

	sendit:
		{
			size_t num;

			memset(buf, 0, sizeof(buf));
			th->th_opcode = htons(DATA);
			th->th_block  = htons((ftell(fp) / SEGSIZE) + 1);
			fprintf(stderr, "tftp reading %d bytes ...\n", SEGSIZE);
			num = fread(th->th_data, sizeof(char), SEGSIZE, fp);
			fprintf(stderr, "tftp sending %zd + %zd bytes ...\n", hdrsz, num);
			sendto(sd, buf, hdrsz + num, 0, (struct sockaddr *)&addr, sizeof(addr));
		}
	} else if (op == ERROR) {
		fprintf(stderr, "tftp ERROR: %hd\n", ntohs(th->th_code));
	} else if (op == ACK) {
		fprintf(stderr, "tftp ACK, block # %hu\n", block);
		if (fp) {
			if (feof(fp)) {
				fclose(fp);
				fp = NULL;
			} else
				goto sendit;
		}
	} else {
		fprintf(stderr, "tftp opcode: %hd\n", op);
		fprintf(stderr, "tftp block#: %hu\n", block);
	}

	return 0;
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */

