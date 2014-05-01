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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "uftpd.h"

static int  do_syslog = 0;
static char log_msg[128];

void logit(int level, int code, const char *fmt, ...)
{
        int len;
        va_list args;

        va_start(args, fmt);
        len = vsnprintf(log_msg, sizeof(log_msg) - len, fmt, args);
        va_end(args);
        if (code)
                snprintf(log_msg + len, sizeof(log_msg) - len, ". Error %d: %s", code, strerror(code));

	if (!do_syslog) {
		FILE *file = fopen(UFTPD_LOGFILE, "a");

		fprintf(file, "%s\n", log_msg);

		fclose(file);
	} else {
		syslog(level | LOG_FTP, "%s", log_msg);
	}
}

void show_log(char *msg)
{
	logit(LOG_DEBUG, 0, msg);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
