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

#ifndef UFTPD_H_
#define UFTPD_H_

#define UFTPD_IDENT   "uftpd"
#define UFTPD_LOGFILE "uftpd.log"

#define ERROR(code, fmt, args...)   logit(LOG_ERR, code, fmt, ##args)
#define WARNING(code, fmt, args...) logit(LOG_WARNING, code, fmt, ##args)
#define INFO(code, fmt, args...)    logit(LOG_INFO, code, fmt, ##args)
#define DEBUG(code, fmt, args...)   logit(LOG_DEBUG, code, fmt, ##args)

void show_log(char *msg);
void logit(int severity, int code, const char *fmt, ...);

#endif  /* UFTPD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
