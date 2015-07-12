/* uftpd -- the no nonsense (T)FTP server
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

#ifndef FTPCMD_H_
#define FTPCMD_H_

#include "uftpd.h"

void handle_USER(ctrl_t *ctrl, char *name);
void handle_PASS(ctrl_t *ctrl, char *pass);

void handle_SYST(ctrl_t *ctrl);
void handle_TYPE(ctrl_t *ctrl, char *argument);

void handle_PWD(ctrl_t *ctrl);
void handle_CWD(ctrl_t *ctrl, char *dir);
void handle_XPWD(ctrl_t *ctrl);

void handle_PORT(ctrl_t *ctrl, char *str);
void handle_LIST(ctrl_t *ctrl, char *arg);
void handle_NLST(ctrl_t *ctrl, char *arg);

void handle_PASV(ctrl_t *ctrl);

void handle_RETR(ctrl_t *ctrl, char *file);
void handle_STOR(ctrl_t *ctrl, char *file);
void handle_DELE(ctrl_t *ctrl, char *file);

void handle_MKD(ctrl_t *ctrl);
void handle_RMD(ctrl_t *ctrl);

void handle_SIZE(ctrl_t *ctrl, char *file);

void handle_RNFR(ctrl_t *ctrl);
void handle_RNTO(ctrl_t *ctrl);

void handle_QUIT(ctrl_t *ctrl);
void handle_CLNT(ctrl_t *ctrl);

void handle_OPTS(ctrl_t *ctrl);

#endif  /* FTPCMD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
