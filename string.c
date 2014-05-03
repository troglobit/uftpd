/* uftpd -- the small no nonsense FTP server
 *
 * Copyright (c) 2013-2014  Xu Wang <wangxu.93@icloud.com>
 * Copyright (c)      2014  Joachim Nilsson <troglobit@gmail.com>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "string.h"

//transfport ip and port to buf
//memory be released out of  the function
char *_transfer_ip_port_str(char *ip, int port)
{
	int i;
	char *a, *b, *buf;
	char *dup = strdup(ip);
	size_t len;

	if (!dup)
		return NULL;
	len = strlen(dup);

	buf = calloc(30, sizeof(char));
	if (!buf) {
		free(dup);
		return NULL;
	}

	a = dup;
	for (i = 0; i < 4; i++) {
		b = strpbrk(a, ".");
		if (b) {
			*b = 0;
			b++;
		}
		strcat(buf, a);
		strcat(buf, ",");
		a = b;
	}

	snprintf(dup, len, "%d", port / 256);
	strcat(buf, dup);
	strcat(buf, ",");

	snprintf(dup, len, "%d", port % 256);
	strcat(buf, dup);

	free(dup);

	return buf;
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
