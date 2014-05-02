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

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE       1000
#define FTP_DEFAULT_PORT  21
#define FTP_DEFAULT_HOME  "/srv/ftp"

struct context {
	int sd;
	int port;
	char home[100];
	char address[INET_ADDRSTRLEN];
};

struct controller {
	int sd;
	int type;

	char name[20];
	char pass[20];

	/* PASV */
	int data_sd;
	int data_listen_sd;

	/* PORT */
	char data_address[INET_ADDRSTRLEN];
	int  data_port;

	char home[100];
	char cwd[100];

	char address[INET_ADDRSTRLEN];

	int status;
};

void init_defaults(struct context *ctx);
int  serve_files(struct context *ctx);

//handle command
void handle_client_command(struct controller *ctrl);

//send file
int send_file(struct controller *ctrl, FILE * file);

//send message
void send_msg(int socket, char *msg);

//receive message
void recv_msg(int socket, char *buf, size_t len, char **cmd, char **arguments);

//
int check_user_pass(struct controller *ctrl);

//printf log
void show_log(char *log);

//
int establish_tcp_connection(struct controller *ctrl);

//
void cancel_tcp_connection(struct controller *ctrl);

//
void handle_USER(struct controller *ctrl, char *name);

//
void handle_PASS(struct controller *ctrl, char *pass);

//
void handle_SYST(struct controller *ctrl);

//
void handle_TYPE(struct controller *ctrl, char *argument);

//void
void handle_PWD(struct controller *ctrl);

//
void handle_CWD(struct controller *ctrl, char *dir);

//
//void handle_XPWD(struct controller *ctrl);
//
void handle_PORT(struct controller *ctrl, char *str);

//
void handle_LIST(struct controller *ctrl);

//
void handle_PASV(struct controller *ctrl);

//
void *handle_RETR(void *ctrl);

//
void handle_STOR(struct controller *ctrl, char *path);

//
void handle_DELE(struct controller *ctrl, char *path);

//
void handle_MKD(struct controller *ctrl);

//
void handle_RMD(struct controller *ctrl);

//
void handle_SIZE(struct controller *ctrl, char *file);

//
void handle_RNFR(struct controller *ctrl);

//
void handle_RNTO(struct controller *ctrl);

//
void handle_QUIT(struct controller *ctrl);

//
void handle_CLNT(struct controller *ctrl);

//
void handle_OPTS(struct controller *ctrl);

#endif  /* FTPCMD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
