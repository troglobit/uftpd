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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "fops.h"
#include "ftpcmd.h"
#include "string.h"

#define TRUE 1

//Init socket
void init_ftp_server(struct FtpServer *ftp)
{
	//strcpy(ftp->_relative_path, "/home/xu");//root path
	ftp->_socket = socket(AF_INET, SOCK_STREAM, 0);
	int err, sock_reuse = 1;

	err = setsockopt(ftp->_socket, SOL_SOCKET, SO_REUSEADDR,
			 (char *)&sock_reuse, sizeof(sock_reuse));
	if (err != 0) {
		perror("Failed setting SO_REUSEADDR");
		exit(1);
	}
	if (ftp->_socket < 0) {
		perror("opening socket error");
		exit(1);
	}
	ftp->_server.sin_family = AF_INET;
	ftp->_server.sin_addr.s_addr = INADDR_ANY;
	ftp->_server.sin_port = htons(ftp->_port);
	if (bind(ftp->_socket, (struct sockaddr *)&ftp->_server,
		 sizeof(struct sockaddr)) < 0) {
		perror("binding error");
		exit(1);
	}
	show_log("server is estabished. Waiting for connnect...");
}

//start socket listening
void start_ftp_server(struct FtpServer *ftp)
{
//default watch over 20 sockets
	char log[100];

	listen(ftp->_socket, 20);
	socklen_t size = sizeof(struct sockaddr);

	while (1) {
		int client;
		struct sockaddr_in client_addr;

		client =
		    accept(ftp->_socket, (struct sockaddr *)&client_addr,
			   &size);
		if (client < 0) {
			perror("accept error");
		} else {
			socklen_t sock_length = sizeof(struct sockaddr);
			char host_ip[100];
			char client_ip[100];

			//get host ip
			struct sockaddr_in host_addr;

			getsockname(client, (struct sockaddr *)&host_addr,
				    &sock_length);
			inet_ntop(AF_INET, &(host_addr.sin_addr), host_ip,
				  INET_ADDRSTRLEN);
			strcpy(ftp->_ip, host_ip);
			//printf("%s", ftp->_ip);
			getpeername(client, (struct sockaddr *)&client_addr,
				    &sock_length);
			inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip,
				  INET_ADDRSTRLEN);
			sprintf(log, "%s connect to the host.", client_ip);
			show_log(log);

		}
		struct FtpClient *_c =
		    (struct FtpClient *)malloc(sizeof(struct FtpClient));
		init_ftp_client(_c, ftp, client);
		pthread_t pid;

		pthread_create(&pid, NULL, communication, (void *)(_c));
		//close(client);
	}
}

void set_ftp_server_port(struct FtpServer *ftp, int port)
{
	ftp->_port = port;
}

//initial FtpClient
void init_ftp_client(struct FtpClient *client, struct FtpServer *server,
		     int client_socket)
{
	client->_client_socket = client_socket;
	strcpy(client->_ip, server->_ip);
	strcpy(client->_root, server->_relative_path);
	strcpy(client->_cur_path, "/");
	client->_type = 1;
	client->status = 0;
	client->_data_server_socket = -1;
	client->_data_socket = -1;
	client->_name[0] = 0;
	client->_pass[0] = 0;
	client->_dataip[0] = 0;

}

//Communication 
void *communication(void *c)
{
	struct FtpClient *client = (struct FtpClient *)c;
	int client_socket = client->_client_socket;
	char str[] = "220 Anonymous FTP server ready.\r\n";

	send_msg(client_socket, str);
	handle_client_command(client);
	return NULL;
}

//handle command
void handle_client_command(struct FtpClient *client)
{
	int client_socket = client->_client_socket;
	size_t len = BUFFER_SIZE * sizeof(char);
	char *buffer;
	char *cmd;
	char *argument;

	buffer = malloc(len);
	if (!buffer) {
                perror("Out of memory");
                exit(1);
	}

	while (TRUE) {
		recv_msg(client_socket, buffer, len, &cmd, &argument);
		show_log(cmd);
		show_log(argument);
		if (strcmp("USER", cmd) == 0) {
			handle_USER(client, argument);
		} else if (strcmp("PASS", cmd) == 0) {
			handle_PASS(client, argument);
		} else if (strcmp("SYST", cmd) == 0) {
			handle_SYST(client);
		} else if (strcmp("TYPE", cmd) == 0) {
			handle_TYPE(client);
		} else if (strcmp("PORT", cmd) == 0) {
			handle_PORT(client, argument);
		} else if (strcmp("RETR", cmd) == 0) {
			struct FtpRetr *retr = malloc(sizeof(struct FtpRetr));
			retr->client = client;
			strcpy(retr->path, argument);
			pthread_t pid;

			pthread_create(&pid, NULL, handle_RETR, (void *)retr);
		} else if (strcmp("PASV", cmd) == 0) {
			handle_PASV(client);
		} else if (strcmp("QUIT", cmd) == 0) {
			handle_QUIT(client);
			break;
		} else if (strcmp("LIST", cmd) == 0) {
			handle_LIST(client);
		} else if (strcmp("CLNT", cmd) == 0) {
			handle_CLNT(client);
		} else if (strcmp("OPTS", cmd) == 0) {
			handle_OPTS(client);
		} else if (strcmp("PWD", cmd) == 0) {
			handle_PWD(client);
		} else if (strcmp("STOR", cmd) == 0) {
			handle_STOR(client, argument);
		} else if (strcmp("CWD", cmd) == 0) {
			handle_CWD(client, argument);
		} else {
			char buf[100];

			strcpy(buf, "500 ");
			strcat(buf, cmd);
			strcat(buf, "cannot be recognized by server\r\n");
			send_msg(client->_client_socket, buf);
		}
	}

	free(buffer);
}

//send message
void send_msg(int socket, char *msg)
{
	int l = strlen(msg);

	if (l <= 0) {
		show_log("no message in char* msg");
	}
	int n = 0;

	while (n < l) {
		n += send(socket, msg + n, l, 0);
	}
	if (n < 0) {
		perror("send msg error");
	} else {
		show_log(msg);
	}
}

/*
 * Receive message from client, split into command and argument
 */
void recv_msg(int sd, char *buf, size_t len, char **cmd, char **argument)
{
	char *ptr;
	ssize_t bytes;

	/* Clear for every new command. */
	memset(buf, 0, len);

	bytes = recv(sd, buf, len, 0);
	if (!bytes) {
		show_log("Client disconnected.");
		pthread_exit(NULL);
		return;		/* Dummy */
	}

	if (bytes < 0) {
		perror("Failed reading client command");
		return;
	}

	*cmd = buf;
	ptr  = strpbrk(buf, " ");
	if (ptr) {
		*ptr = 0;
		ptr++;
		*argument = ptr;
	} else {
		*argument = NULL;
		ptr = buf;
	}

	ptr = strpbrk(ptr, "\r\n");
	if (ptr)
		*ptr = 0;
}

void show_log(char *log)
{
	if (log) {
		FILE *file = fopen("uftpd.log", "a");

		fwrite(log, 1, strlen(log), file);
		fclose(file);
	}
}

//
int establish_tcp_connection(struct FtpClient *client)
{
	if (client->_dataip[0]) {
		client->_data_socket = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in servaddr;

		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(client->_dataport);
		if (inet_aton(client->_dataip, &(servaddr.sin_addr)) <= 0) {
			printf("error in port command");
			return -1;
		}
		if (connect(client->_data_socket, (struct sockaddr *)&servaddr,
			    sizeof(struct sockaddr)) == -1) {
			perror("connect");
			return -1;
		}
		show_log("port connect success.");

	} else if (client->_data_server_socket > 0) {
		socklen_t sock = sizeof(struct sockaddr);
		struct sockaddr_in data_client;

		client->_data_socket = accept(client->_data_server_socket,
					      (struct sockaddr *)&data_client,
					      &sock);

		if (client->_data_socket < 0) {
			perror("accept error");
		} else {
			socklen_t sock_length = sizeof(struct sockaddr);
			char client_ip[100];
			char log[100];

			getpeername(client->_data_socket,
				    (struct sockaddr *)&data_client,
				    &sock_length);
			inet_ntop(AF_INET, &(data_client.sin_addr), client_ip,
				  INET_ADDRSTRLEN);
			sprintf(log, "%s connect to the host.", client_ip);
			show_log(log);
		}

	}
	return 1;

}

//
void cancel_tcp_connection(struct FtpClient *client)
{

	if (client->_data_server_socket > 0) {
		close(client->_data_server_socket);
		client->_data_server_socket = -1;
	}
	if (client->_data_socket > 0) {
		close(client->_data_socket);
		client->_data_socket = -1;
	}
	if (client->_dataip[0]) {
		client->_dataip[0] = 0;
		client->_dataport = 0;
	}
}

//
int send_file(struct FtpClient *client, FILE * file)
{
	char buf[1000];

	while (!feof(file)) {
		fread(file, 1000, 1, file);
		send(client->_data_socket, buf, strlen(buf), 0);
	}
	return 0;
}

//redefine write
void handle_USER(struct FtpClient *client, char *name)
{
	if (client->_name[0]) {
		client->_name[0] = 0;
		client->_pass[0] = 0;
	}

	if (name) {
		strcpy(client->_name, name);
		if (check_user_pass(client) == 1) {
			send_msg(client->_client_socket,
				 "230 Guest login OK, access restrictions apply.\r\n");
		} else {
			send_msg(client->_client_socket,
				 "331 Login OK, please enter password.\r\n");
		}
	} else {
		send_msg(client->_client_socket,
			 "530 You must input your name.\r\n");
	}

}

//
void handle_PASS(struct FtpClient *client, char *pass)
{
	if (client->_name[0] == 0) {
		send_msg(client->_client_socket,
			 "503 Your haven't input your username\r\n");
		return;
	}

	strcpy(client->_pass, pass);
	if (check_user_pass(client) < 0) {
		send_msg(client->_client_socket,
			 "530 username or password is unacceptable\r\n");
		return;
	}

	/*send_msg(client_socket, "230-\r\n");
	  send_msg(client_socket, "230-Welcome to\r\n");
	  send_msg(client_socket, "230- School of Software\r\n");
	  send_msg(client_socket, "230-\r\n");
	  send_msg(client_socket, "230-This site is provided as a public service by School of\r\n");
	  send_msg(client_socket, "230-Software. Use in violation of any applicable laws is strictly\r\n");
	  send_msg(client_socket, "230-prohibited. We make no guarantees, explicit or implicit, about the\r\n");
	  send_msg(client_socket, "230-contents of this site. Use at your own risk.\r\n");
	  send_msg(client_socket, "230-\r\n"); */
	send_msg(client->_client_socket,
		 "230 Guest login OK, access restrictions apply.\r\n");
}

//
void handle_SYST(struct FtpClient *client)
{
	char system[] = "215 UNIX Type: L8\r\n";

	send_msg(client->_client_socket, system);
}

//
void handle_TYPE(struct FtpClient *client)
{
	char type[] = "200 Type set to I.\r\n";

	send_msg(client->_client_socket, type);
}

//
void handle_PWD(struct FtpClient *client)
{
	char buf[300];

	strcpy(buf, "257 \"");
	strcat(buf, client->_cur_path);
	strcat(buf, "\"\r\n");
	send_msg(client->_client_socket, buf);
}

//
void handle_CWD(struct FtpClient *client, char *_dir)
{
	int flag = 0;

	if (_dir[0] != '/') {
		flag = 1;
	}
	char dir[300];

	strlcpy(dir, client->_root, sizeof(dir));
	if (flag) {
		strcat(dir, "/");
	}
	show_log("cwd:start");
	show_log(_dir);
	show_log("cwd:end");
	show_log(dir);
	strlcat(dir, _dir, sizeof(dir));
	show_log(dir);
	if (is_exist_dir(dir)) {
		show_log(dir);
		if (flag) {
			strlcpy(client->_cur_path, "/", sizeof(client->_cur_path));
			strlcat(client->_cur_path, _dir, sizeof(client->_cur_path));
		} else {
			strlcat(client->_cur_path, _dir, sizeof(client->_cur_path));
			show_log(client->_cur_path);
		}
		send_msg(client->_client_socket, "250 Okay.\r\n");
	} else {
		send_msg(client->_client_socket,
			 "550 No such file or directory.\r\n");
	}
}

//
void handle_PORT(struct FtpClient *client, char *str)
{
	if (client->_data_socket > 0) {
		close(client->_data_socket);
	}
	client->_dataip[0] = 0;
	int a, b, c, d, e, f;

	sscanf(str, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &e, &f);
	sprintf(client->_dataip, "%d.%d.%d.%d", a, b, c, d);
	client->_dataport = e * 256 + f;
	show_log(client->_dataip);
	//show_log(client->_dataport);

	char connect[] = "200 PORT command successful.\r\n";

	send_msg(client->_client_socket, connect);
}

//
void handle_LIST(struct FtpClient *client)
{
	char path[200];
	char list_cmd_info[200];
	FILE *pipe_fp = NULL;

	show_log(client->_cur_path);

	strlcpy(path, client->_root, sizeof(path));
	strlcat(path, client->_cur_path, sizeof(path));
	sprintf(list_cmd_info, "ls -lgA %s", path);
	show_log(list_cmd_info);

	pipe_fp = popen(list_cmd_info, "r");
	if (!pipe_fp) {
		show_log("pipe open error in cmd_list\n");
		send_msg(client->_client_socket,
			 "451 the server had trouble reading the directory from disk\r\n");
		return;
	}

	char log[100];

	sprintf(log, "pipe open successfully!, cmd is %s.", list_cmd_info);
	show_log(log);
	if (establish_tcp_connection(client)) {
		show_log("establish tcp socket");
	} else {
		send_msg(client->_client_socket,
			 "425 TCP connection cannot be established.\r\n");
	}
	send_msg(client->_client_socket,
		 "150 Data connection accepted; transfer starting.\r\n");

	while (!feof(pipe_fp)) {
		char *ptr;
		char buf[BUFFER_SIZE];

		fgets(buf, sizeof(buf) - 2, pipe_fp);
		ptr = strchr(buf, '\n');
		if (ptr)
			strcpy(ptr, "\r\n");
		else
			strcat(buf, "\r\n");

		send_msg(client->_data_socket, buf);
	}

	pclose(pipe_fp);
	send_msg(client->_client_socket, "226 Transfer complete.\r\n");
	cancel_tcp_connection(client);
}

//
void handle_PASV(struct FtpClient *client)
{
	int port;
	char *msg, buf[200];
	struct sockaddr_in server;
	struct sockaddr_in file_addr;
	socklen_t file_sock_len = sizeof(struct sockaddr);

	if (client->_data_socket > 0) {
		close(client->_data_socket);
		client->_data_socket = -1;
	}

	if (client->_data_server_socket > 0)
		close(client->_data_server_socket);

	client->_data_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client->_data_server_socket < 0) {
		perror("opening socket error");
		send_msg(client->_client_socket, "426 pasv failure\r\n");
		return;
	}

	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = inet_addr(client->_ip);
	server.sin_port        = htons(0);
	if (bind(client->_data_server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
		perror("Failed binding to client socket");
		send_msg(client->_client_socket, "426 PASV failure\r\n");
		return;
	}

	show_log("server is estabished. Waiting for connnect...");
	if (listen(client->_data_server_socket, 1) < 0) {
		perror("listen error");
		send_msg(client->_client_socket, "426 PASV failure\r\n");
	}

	getsockname(client->_data_server_socket, (struct sockaddr *)&file_addr, &file_sock_len);
	show_log(client->_ip);

	port = ntohs(file_addr.sin_port);
	msg = _transfer_ip_port_str(client->_ip, port);
	if (!msg) {
		send_msg(client->_client_socket, "426 PASV failure\r\n");
		exit(1);
	}

	snprintf(buf, sizeof(buf), "Port %d\n", port);
	show_log(buf);

	strcpy(buf, "227 Entering Passive Mode (");
	strcat(buf, msg);
	strcat(buf, ")\r\n");
	show_log(buf);
	send_msg(client->_client_socket, buf);

	free(msg);
}

//
void *handle_RETR(void *retr)
{
	FILE *file = NULL;
	char path[200];
	char _path[400];
	struct FtpRetr *re = (struct FtpRetr *)retr;
	struct FtpClient *client = re->client;

	strcpy(path, re->path);
	//establish_tcp_connection(client);

	strcpy(_path, client->_root);
	strcat(_path, client->_cur_path);
	if (_path[strlen(_path) - 1] != '/')
		strcat(_path, "/");
	strcat(_path, path);
	show_log(_path);

	file = fopen(_path, "rb");
	if (!file) {
		fprintf(stderr, "Failed fopen(%s): %s", _path, strerror(errno));
		send_msg(client->_client_socket,
			 "451 trouble to retr file\r\n");
		return NULL;
	}

	if (establish_tcp_connection(client) > 0) {
		send_msg(client->_client_socket,
			 "150 Data connection accepted; transfer starting.\r\n");
		char buf[1000];

		while (!feof(file)) {
			int n = fread(buf, 1, 1000, file);
			int j = 0;

			while (j < n) {
				j += send(client->_data_socket, buf + j, n - j,
					  0);
			}

			//      send_msg(client->_client_socket, "426 TCP connection was established but then broken\r\n");
			//return;

		}

		fclose(file);
		cancel_tcp_connection(client);

		send_msg(client->_client_socket, "226 Transfer ok.\r\n");
	} else {
		send_msg(client->_client_socket,
			 "425 TCP connection cannot be established.\r\n");
	}
	pthread_exit(NULL);
	return NULL;
}

//
void handle_STOR(struct FtpClient *client, char *path)
{
	//establish_tcp_connection(client);
	FILE *file = NULL;
	char _path[400];

	strcpy(_path, client->_root);
	strcat(_path, client->_cur_path);
	if (_path[strlen(_path) - 1] != '/') {
		strcat(_path, "/");
	}
	strcat(_path, path);

	file = fopen(_path, "wb");
	show_log(_path);

	if (file == NULL) {
		send_msg(client->_client_socket,
			 "451 trouble to stor file\r\n");
		return;
	}

	if (establish_tcp_connection(client) > 0) {
		send_msg(client->_client_socket,
			 "150 Data connection accepted; transfer starting.\r\n");
		char buf[1000];
		int j = 0;

		while (1) {
			j = recv(client->_data_socket, buf, 1000, 0);
			if (j == 0) {
				cancel_tcp_connection(client);
				break;
			}
			if (j < 0) {
				send_msg(client->_client_socket,
					 "426 TCP connection was established but then broken\r\n");
				return;
			}
			fwrite(buf, 1, j, file);

		}
		cancel_tcp_connection(client);
		fclose(file);

		send_msg(client->_client_socket, "226 stor ok.\r\n");
	} else {
		send_msg(client->_client_socket,
			 "425 TCP connection cannot be established.\r\n");
	}
}

//
void handle_DELE(struct FtpClient *client, char *name)
{

}

//
void handle_MKD(struct FtpClient *client)
{

}

//
void handle_RMD(struct FtpClient *client)
{

}

//
void handle_SIZE(struct FtpClient *client)
{

}

//
void handle_RNFR(struct FtpClient *client)
{

}

//
void handle_RNTO(struct FtpClient *client)
{

}

//
void handle_QUIT(struct FtpClient *client)
{
	send_msg(client->_client_socket, "221 goodby~\r\n");
}

//
void handle_CLNT(struct FtpClient *client)
{
	send_msg(client->_client_socket, "200 CLNT\r\n");
}

//
void handle_OPTS(struct FtpClient *client)
{
	send_msg(client->_client_socket, "200 UTF8 OPTS ON\r\n");
}

//
int check_user_pass(struct FtpClient *client)
{
	if (!client->_name)
		return -1;

	if (strcmp("anonymous", client->_name) == 0)
		return 1;

	return 0;
}

//
void free_ftp_client(struct FtpClient *client)
{
	if (client->_client_socket > 0)
		close(client->_client_socket);

	if (client->_data_server_socket > 0)
		close(client->_data_server_socket);
	if (client->_data_socket > 0)
		close(client->_data_socket);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
