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

#define BUFFER_SIZE 1000
#define FTP_PORT 8000

struct FtpServer {
	int _socket;
	struct sockaddr_in _server;
	int _port;
	char _relative_path[100];	//ftp server root path
	char _ip[20];
};

struct FtpClient {
	int _client_socket;
	int _data_socket;
	char _cur_path[100];
	char _dataip[20];
	int _dataport;
	int _type;
	char _root[100];
	char _ip[20];
	char _name[20];
	char _pass[20];
	int _data_server_socket;

	int status;
};

struct FtpRetr {
	struct FtpClient *client;
	char path[200];
};

//Init socket
void init_ftp_server(struct FtpServer *ftp);

//start socket listening
void start_ftp_server(struct FtpServer *ftp);

//
//void  close_ftp_server(struct FtpServer* ftp);
//reset port
void set_ftp_server_port(struct FtpServer *ftp, int port);

//communication
void *communication(void *client_socket);

//initial FtpClient
void init_ftp_client(struct FtpClient *client, struct FtpServer *server,
		     int client_socket);
//
void free_ftp_client(struct FtpClient *client);

//handle command
void handle_client_command(struct FtpClient *client);

//send file
int send_file(struct FtpClient *client, FILE * file);

//send message
void send_msg(int socket, char *msg);

//receive message
void recv_msg(int socket, char *buf, size_t len, char **cmd, char **arguments);

//
int check_user_pass(struct FtpClient *client);

//printf log
void show_log(char *log);

//
int establish_tcp_connection(struct FtpClient *client);

//
void cancel_tcp_connection(struct FtpClient *client);

//
void handle_USER(struct FtpClient *client, char *name);

//
void handle_PASS(struct FtpClient *client, char *pass);

//
void handle_SYST(struct FtpClient *client);

//
void handle_TYPE(struct FtpClient *client);

//void
void handle_PWD(struct FtpClient *client);

//
void handle_CWD(struct FtpClient *client, char *dir);

//
//void handle_XPWD(struct FtpClient* client);
//
void handle_PORT(struct FtpClient *client, char *str);

//
void handle_LIST(struct FtpClient *client);

//
void handle_PASV(struct FtpClient *client);

//
void *handle_RETR(void *client);

//
void handle_STOR(struct FtpClient *client, char *path);

//
void handle_DELE(struct FtpClient *client, char *path);

//
void handle_MKD(struct FtpClient *client);

//
void handle_RMD(struct FtpClient *client);

//
void handle_SIZE(struct FtpClient *client);

//
void handle_RNFR(struct FtpClient *client);

//
void handle_RNTO(struct FtpClient *client);

//
void handle_QUIT(struct FtpClient *client);

//
void handle_CLNT(struct FtpClient *client);

//
void handle_OPTS(struct FtpClient *client);

#endif  /* FTPCMD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
