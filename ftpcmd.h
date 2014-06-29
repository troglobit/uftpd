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

#ifndef FTPCMD_H_
#define FTPCMD_H_

#include "uftpd.h"

void handle_USER(ctx_t *ctrl, char *name);
void handle_PASS(ctx_t *ctrl, char *pass);

void handle_SYST(ctx_t *ctrl);
void handle_TYPE(ctx_t *ctrl, char *argument);

void handle_PWD(ctx_t *ctrl);
void handle_CWD(ctx_t *ctrl, char *dir);
void handle_XPWD(ctx_t *ctrl);

void handle_PORT(ctx_t *ctrl, char *str);
void handle_LIST(ctx_t *ctrl);

void handle_PASV(ctx_t *ctrl);

void handle_RETR(ctx_t *ctrl, char *file);
void handle_STOR(ctx_t *ctrl, char *file);
void handle_DELE(ctx_t *ctrl, char *file);

void handle_MKD(ctx_t *ctrl);
void handle_RMD(ctx_t *ctrl);

void handle_SIZE(ctx_t *ctrl, char *file);

void handle_RNFR(ctx_t *ctrl);
void handle_RNTO(ctx_t *ctrl);

void handle_QUIT(ctx_t *ctrl);
void handle_CLNT(ctx_t *ctrl);

void handle_OPTS(ctx_t *ctrl);

#endif  /* FTPCMD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
