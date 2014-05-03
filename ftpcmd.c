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

#include "uftpd.h"
#include "fops.h"

static int  sd = -1;

struct ftp_retr {
	ctx_t *ctrl;
	char path[200];
};

static void start_session(int sd, char *ouraddr, char *hisaddr);

int serve_files(void)
{
	int err, val = 1;
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in server;

	sd = socket(AF_INET, SOCK_STREAM, 0);
	if (sd < 0) {
		ERR(errno, "Failed creating FTP server socket");
		exit(1);
	}

	err = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));
	if (err != 0) {
		ERR(errno, "Failed setting SO_REUSEADDR");
		return 1;
	}

	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port        = htons(port);
	if (bind(sd, (struct sockaddr *)&server, len) < 0) {
		ERR(errno, "Failed binding to port %d, maye another FTP server is already running", port);
		return 1;
	}

	if (-1 == listen(sd, 20)) {
		ERR(errno, "Failed starting FTP server");
		return 1;
	}

	LOG("Serving files from %s, listening on port %d ...", home, port);

	while (1) {
		int client;	/* Client socket */
		char ouraddr[INET_ADDRSTRLEN], hisaddr[INET_ADDRSTRLEN];;
		socklen_t len = sizeof(struct sockaddr);
		struct sockaddr_in host_addr;
		struct sockaddr_in client_addr;

		client = accept(sd, (struct sockaddr *)&client_addr, &len);
		if (client < 0) {
			perror("Failed accepting incoming client connection");
			continue;
		}

		/* Find our address */
		len = sizeof(struct sockaddr);
		getsockname(client, (struct sockaddr *)&host_addr, &len);
		inet_ntop(AF_INET, &(host_addr.sin_addr), ouraddr, INET_ADDRSTRLEN);

		/* Find peer address */
		len = sizeof(struct sockaddr);
		getpeername(client, (struct sockaddr *)&client_addr, &len);
		inet_ntop(AF_INET, &(client_addr.sin_addr), hisaddr, INET_ADDRSTRLEN);
		LOG("Client connection from %s", hisaddr);

		start_session(client, ouraddr, hisaddr);
	}

	return 0;
}

static void send_msg(int sd, char *msg)
{
	int n = 0;
	int l;

	if (!msg) {
	err:
		ERR(EINVAL, "Missing argument to send_msg()");
		return;
	}

	l = strlen(msg);
	if (l <= 0)
		goto err;

	while (n < l) {
		int result = send(sd, msg + n, l, 0);

		if (result < 0) {
			perror("Failed sending message to client");
			return;
		}

		n += result;
	}

	DBG("%s\n", msg);
}

/*
 * Receive message from client, split into command and argument
 */
void recv_msg(int sd, char *msg, size_t len, char **cmd, char **argument)
{
	char *ptr;
	ssize_t bytes;

	/* Clear for every new command. */
	memset(msg, 0, len);

	bytes = recv(sd, msg, len, 0);
	if (!bytes) {
		show_log("Client disconnected.");
		pthread_exit(NULL);
		return;		/* Dummy */
	}

	if (bytes < 0) {
		perror("Failed reading client command");
		return;
	}

	*cmd = msg;
	ptr  = strpbrk(msg, " ");
	if (ptr) {
		*ptr = 0;
		ptr++;
		*argument = ptr;
	} else {
		*argument = NULL;
		ptr = msg;
	}

	ptr = strpbrk(ptr, "\r\n");
	if (ptr)
		*ptr = 0;
}

static int open_data_connection(ctx_t *ctrl)
{
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in sin;

	/* Previous PORT command from client */
	if (ctrl->data_address[0]) {
		ctrl->data_sd = socket(AF_INET, SOCK_STREAM, 0);

		sin.sin_family = AF_INET;
		sin.sin_port = htons(ctrl->data_port);
		inet_aton(ctrl->data_address, &(sin.sin_addr));

		if (connect(ctrl->data_sd, (struct sockaddr *)&sin, len) == -1) {
			perror("connect");
			return -1;
		}

		DBG("Connected successfully to client's previously requested address:PORT %s:%s", ctrl->data_address, ctrl->data_port);
		return 0;
	}

	/* Previous PASV command, accept connect from client */
	if (ctrl->data_listen_sd > 0) {
		ctrl->data_sd = accept(ctrl->data_listen_sd, (struct sockaddr *)&sin, &len);

		if (ctrl->data_sd < 0) {
			perror("accept error");
		} else {
			char client_ip[100];

			len = sizeof(struct sockaddr);
			getpeername(ctrl->data_sd, (struct sockaddr *)&sin, &len);
			inet_ntop(AF_INET, &(sin.sin_addr), client_ip, INET_ADDRSTRLEN);
			DBG("Client PASV data connection from %s", client_ip);
		}
	}

	return 0;
}

static void close_data_connection(ctx_t *ctrl)
{
	/* PASV server listening socket */
	if (ctrl->data_listen_sd > 0) {
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
	}

	/* PASV client socket */
	if (ctrl->data_sd > 0) {
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
	}

	/* PORT */
	if (ctrl->data_address[0]) {
		ctrl->data_address[0] = 0;
		ctrl->data_port = 0;
	}
}

static char *compose_path(ctx_t *ctrl, char *file, char *path, size_t len)
{
	strlcpy(path, ctrl->home, len);
	strlcat(path, ctrl->cwd, len);

	DBG("home: %s, cwd: %s, path: %s, file: %s, strlen(path): %zd",
	    ctrl->home, ctrl->cwd, path, file, strlen(path));

	if (path[strlen(path) - 1] != '/')
		strlcat(path, "/", len);

	if (file[0] == '/')
		file++;

	strlcat(path, file, len);

	return path;
}

static void handle_command(ctx_t *ctrl)
{
	int client_socket = ctrl->sd;
	size_t len = BUFFER_SIZE * sizeof(char);
	char *buffer;
	char *cmd;
	char *argument;

	buffer = malloc(len);
	if (!buffer) {
                perror("Out of memory");
                exit(1);
	}

	snprintf(buffer, len, "220 %s (%s) ready.\r\n", __progname, VERSION);
	send_msg(ctrl->sd, buffer);

	while (1) {
		recv_msg(client_socket, buffer, len, &cmd, &argument);
		show_log(cmd);
		show_log(argument);
		if (strcmp("USER", cmd) == 0) {
			handle_USER(ctrl, argument);
		} else if (strcmp("PASS", cmd) == 0) {
			handle_PASS(ctrl, argument);
		} else if (strcmp("SYST", cmd) == 0) {
			handle_SYST(ctrl);
		} else if (strcmp("TYPE", cmd) == 0) {
			handle_TYPE(ctrl, argument);
		} else if (strcmp("PORT", cmd) == 0) {
			handle_PORT(ctrl, argument);
		} else if (strcmp("RETR", cmd) == 0) {
			pthread_t pid;
			struct ftp_retr *retr = malloc(sizeof(struct ftp_retr));

			if (!retr) {
				ERR(errno, "Failed allocating memory for RETR operation");
				send_msg(ctrl->sd, "451 Server out of memory error on RETR.\r\n");
				continue;
			}

			retr->ctrl = ctrl;
			strlcpy(retr->path, argument, sizeof(retr->path));
			pthread_create(&pid, NULL, handle_RETR, (void *)retr);
		} else if (strcmp("PASV", cmd) == 0) {
			handle_PASV(ctrl);
		} else if (strcmp("QUIT", cmd) == 0) {
			handle_QUIT(ctrl);
			break;
		} else if (strcmp("LIST", cmd) == 0) {
			handle_LIST(ctrl);
		} else if (strcmp("CLNT", cmd) == 0) {
			handle_CLNT(ctrl);
		} else if (strcmp("OPTS", cmd) == 0) {
			handle_OPTS(ctrl);
		} else if (strcmp("PWD", cmd) == 0) {
			handle_PWD(ctrl);
		} else if (strcmp("STOR", cmd) == 0) {
			handle_STOR(ctrl, argument);
		} else if (strcmp("CWD", cmd) == 0) {
			handle_CWD(ctrl, argument);
		} else if (strcmp("SIZE", cmd) == 0) {
			handle_SIZE(ctrl, argument);
		} else if (strcmp("NOOP", cmd) == 0) {
			send_msg(ctrl->sd, "200 NOOP OK.\r\n");
		} else {
			char buf[100];

			snprintf(buf, sizeof(buf), "500 %s cannot be recognized by server\r\n", cmd);
			send_msg(ctrl->sd, buf);
		}
	}

	free(buffer);
}

static void stop_session(ctx_t *ctrl)
{
	if (ctrl->sd > 0)
		close(ctrl->sd);

	if (ctrl->data_listen_sd > 0)
		close(ctrl->data_listen_sd);

	if (ctrl->data_sd > 0)
		close(ctrl->data_sd);

	free(ctrl);
}

static void *session(void *c)
{
	ctx_t *ctrl = (ctx_t *)c;

	handle_command(ctrl);
	stop_session(ctrl);

	return NULL;
}

static void start_session(int sd, char *ouraddr, char *hisaddr)
{
	pthread_t pid;
	ctx_t *ctrl = malloc(sizeof(ctx_t));

	ctrl->sd = sd;
	strlcpy(ctrl->ouraddr, ouraddr, sizeof(ctrl->ouraddr));
	strlcpy(ctrl->hisaddr, hisaddr, sizeof(ctrl->hisaddr));
	strlcpy(ctrl->home, home, sizeof(ctrl->home));
	strlcpy(ctrl->cwd, "/", sizeof(ctrl->cwd));
	ctrl->type = TYPE_A;
	ctrl->status = 0;
	ctrl->data_listen_sd = -1;
	ctrl->data_sd = -1;
	ctrl->name[0] = 0;
	ctrl->pass[0] = 0;
	ctrl->data_address[0] = 0;

	pthread_create(&pid, NULL, session, (void *)ctrl);
}

int check_user_pass(ctx_t *ctrl)
{
	if (!ctrl->name)
		return -1;

	if (strcmp("anonymous", ctrl->name) == 0)
		return 1;

	return 0;
}

void handle_USER(ctx_t *ctrl, char *name)
{
	if (ctrl->name[0]) {
		ctrl->name[0] = 0;
		ctrl->pass[0] = 0;
	}

	if (name) {
		strlcpy(ctrl->name, name, sizeof(ctrl->name));
		if (check_user_pass(ctrl) == 1) {
			LOG("Guest logged in from %s", ctrl->hisaddr);
			send_msg(ctrl->sd, "230 Guest login OK, access restrictions apply.\r\n");
		} else {
			send_msg(ctrl->sd, "331 Login OK, please enter password.\r\n");
		}
	} else {
		send_msg(ctrl->sd, "530 You must input your name.\r\n");
	}
}

void handle_PASS(ctx_t *ctrl, char *pass)
{
	if (ctrl->name[0] == 0) {
		send_msg(ctrl->sd, "503 Your haven't input your username\r\n");
		return;
	}

	strlcpy(ctrl->pass, pass, sizeof(ctrl->pass));
	if (check_user_pass(ctrl) < 0) {
		LOG("User %s from %s, invalid password!", ctrl->name, ctrl->hisaddr);
		send_msg(ctrl->sd, "530 username or password is unacceptable\r\n");
		return;
	}

	LOG("User %s login from %s", ctrl->name, ctrl->hisaddr);
	send_msg(ctrl->sd, "230 Guest login OK, access restrictions apply.\r\n");
}

void handle_SYST(ctx_t *ctrl)
{
	char system[] = "215 UNIX Type: L8\r\n";

	send_msg(ctrl->sd, system);
}

void handle_TYPE(ctx_t *ctrl, char *argument)
{
	char type[24]  = "200 Type set to I.\r\n";
	char unknown[] = "501 Invalid argument to TYPE.\r\n";

	if (!argument)
		argument = "Z";

	switch (argument[0]) {
	case 'A':
		ctrl->type = TYPE_A; /* ASCII */
		break;

	case 'I':
		ctrl->type = TYPE_I; /* IMAGE/BINARY */
		break;

	default:
		send_msg(ctrl->sd, unknown);
		return;
	}

	type[16] = argument[0];
	send_msg(ctrl->sd, type);
}

void handle_PWD(ctx_t *ctrl)
{
	char buf[300];

	snprintf(buf, sizeof(buf), "257 \"%s\"\r\n", ctrl->cwd);
	send_msg(ctrl->sd, buf);
}

void handle_CWD(ctx_t *ctrl, char *_dir)
{
	int flag = 0;
	char dir[300];

	if (_dir && strlen(_dir) == 1 && _dir[0] == '/') {
		ctrl->cwd[0] = 0;
		send_msg(ctrl->sd, "250 OK\r\n");
		return;
	}

	if (_dir[0] != '/')
		flag = 1;

	strlcpy(dir, ctrl->home, sizeof(dir));
	if (flag)
		strlcat(dir, "/", sizeof(dir));

	show_log("cwd:start");
	show_log(_dir);
	show_log("cwd:end");
	show_log(dir);
	strlcat(dir, _dir, sizeof(dir));
	show_log(dir);

	if (!is_exist_dir(dir)) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	show_log(dir);
	if (flag) {
		strlcpy(ctrl->cwd, "/", sizeof(ctrl->cwd));
		strlcat(ctrl->cwd, _dir, sizeof(ctrl->cwd));
	} else {
		strlcat(ctrl->cwd, _dir, sizeof(ctrl->cwd));
		show_log(ctrl->cwd);
	}

	send_msg(ctrl->sd, "250 OK\r\n");
}

void handle_PORT(ctx_t *ctrl, char *str)
{
	int a, b, c, d, e, f;
	char addr[INET_ADDRSTRLEN];
	struct sockaddr_in sin;

	if (ctrl->data_sd > 0)
		close(ctrl->data_sd);

	sscanf(str, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &e, &f);
	sprintf(addr, "%d.%d.%d.%d", a, b, c, d);
	if (!inet_aton(addr, &(sin.sin_addr))) {
		ERR(0, "Invalid address '%s' given to PORT command", addr);
		send_msg(ctrl->sd, "200 PORT command successful.\r\n");
		return;
	}

	strlcpy(ctrl->data_address, addr, sizeof(ctrl->data_address));
	ctrl->data_port = e * 256 + f;

	DBG("Client PORT command accepted for %s:%s", ctrl->data_address, ctrl->data_port);
	send_msg(ctrl->sd, "200 PORT command successful.\r\n");
}

void handle_LIST(ctx_t *ctrl)
{
	char *buf;
	char path[200];
	FILE *pipe_fp = NULL;
	size_t len = BUFFER_SIZE * sizeof(char);

	show_log(ctrl->cwd);

	strlcpy(path, ctrl->home, sizeof(path));
	strlcat(path, ctrl->cwd, sizeof(path));

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	snprintf(buf, len, "LC_TIME=C ls -lnA %s", path);
	pipe_fp = popen(buf, "r");
	if (!pipe_fp) {
		free(buf);
		show_log("Failed opening LIST command pipe!");
		send_msg(ctrl->sd, "451 Internal server error.\r\n");
		return;
	}

	if (open_data_connection(ctrl)) {
		free(buf);
		pclose(pipe_fp);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		return;
	}

	show_log("Established TCP socket for data communication.");
	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");

	while (!feof(pipe_fp)) {
		char *ptr, *pos;

		pos = buf;
		while (fgets(pos, len - (pos - buf) - 1, pipe_fp)) {
			if (!strncmp(pos, "total ", 6))
				continue;

			if (ctrl->type == TYPE_A) {
				ptr = strchr(pos, '\n');
				if (ptr)
					strcpy(ptr, "\r\n");
				else
					strcat(pos, "\r\n");
			}

			pos = pos + strlen(pos);
		}
		*pos = 0;
		send_msg(ctrl->data_sd, buf);
	}

	free(buf);
	pclose(pipe_fp);
	close_data_connection(ctrl);
	send_msg(ctrl->sd, "226 Transfer complete.\r\n");
}

/* XXX: Audit this, does it really work with multiple interfaces? */
void handle_PASV(ctx_t *ctrl)
{
	int port;
	char *msg, buf[200];
	struct sockaddr_in server;
	struct sockaddr_in data;
	socklen_t len = sizeof(struct sockaddr);

	if (ctrl->data_sd > 0) {
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
	}

	if (ctrl->data_listen_sd > 0)
		close(ctrl->data_listen_sd);

	ctrl->data_listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (ctrl->data_listen_sd < 0) {
		perror("opening socket error");
		send_msg(ctrl->sd, "426 pasv failure\r\n");
		return;
	}

	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = inet_addr(ctrl->ouraddr);
	server.sin_port        = htons(0);
	if (bind(ctrl->data_listen_sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
		perror("Failed binding to client socket");
		send_msg(ctrl->sd, "426 PASV failure\r\n");
		return;
	}

	show_log("server is estabished. Waiting for connnect...");
	if (listen(ctrl->data_listen_sd, 1) < 0) {
		perror("listen error");
		send_msg(ctrl->sd, "426 PASV failure\r\n");
	}

	getsockname(ctrl->data_listen_sd, (struct sockaddr *)&data, &len);
	port = ntohs(data.sin_port);
	/* XXX: s/ctrl->ouraddr/data.sin_addr.saddr? */
	msg = _transfer_ip_port_str(ctrl->ouraddr, port);
	if (!msg) {
		send_msg(ctrl->sd, "426 PASV failure\r\n");
		exit(1);
	}

	snprintf(buf, sizeof(buf), "Port %d\n", port);
	show_log(buf);

	snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%s)\r\n", msg);
	send_msg(ctrl->sd, buf);

	free(msg);
}

//
void *handle_RETR(void *retr)
{
	int result = 0;
	FILE *fp = NULL;
	char path[200];
	char _path[400];
	char *buf;
	size_t len = BUFFER_SIZE * sizeof(char);
	struct ftp_retr *re = (struct ftp_retr *)retr;
	ctx_t *ctrl = re->ctrl;

	strlcpy(path, re->path, sizeof(path));
	compose_path(ctrl, path, _path, sizeof(_path));
	show_log(_path);

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return NULL;
	}

	fp = fopen(_path, "rb");
	if (!fp) {
		ERR(errno, "Failed opening file %s for RETR", _path);
		free(buf);
		free(re);
		send_msg(ctrl->sd, "451 Trouble to RETR file.\r\n");
		pthread_exit(NULL);
		return NULL;
	}

	if (open_data_connection(ctrl)) {
		fclose(fp);
		free(buf);
		free(re);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		pthread_exit(NULL);
		return NULL;
	}

	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");

	while (!feof(fp) && !result) {
		int n = fread(buf, sizeof(char), len, fp);
		int j = 0;

		while (j < n) {
			ssize_t bytes = send(ctrl->data_sd, buf + j, n - j, 0);

			if (-1 == bytes) {
				ERR(errno, "Failed sending file %s to client", _path);
				result = 1;
				break;
			}
			j += bytes;
		}
	}

	if (result) {
		send_msg(ctrl->sd, "426 TCP connection was established but then broken!\r\n");
	} else {
		LOG("User %s from %s downloaded file %s", ctrl->name, ctrl->hisaddr, _path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
	free(re);
	pthread_exit(NULL);

	return NULL;
}

//
void handle_STOR(ctx_t *ctrl, char *path)
{
	int result = 0;
	FILE *fp = NULL;
	char _path[400];
	char *buf;
	size_t len = BUFFER_SIZE * sizeof(char);

	strlcpy(_path, ctrl->home, sizeof(_path));
	strlcat(_path, ctrl->cwd, sizeof(_path));
	if (_path[strlen(_path) - 1] != '/')
		strlcat(_path, "/", sizeof(_path));
	strlcat(_path, path, sizeof(_path));
	DBG("STOR %s", _path);

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	fp = fopen(_path, "wb");
	if (!fp) {
		free(buf);
		send_msg(ctrl->sd, "451 Trouble storing file.\r\n");
		return;
	}

	if (open_data_connection(ctrl)) {
		fclose(fp);
		free(buf);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		return;
	}

	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");
	while (1) {
		int j = recv(ctrl->data_sd, buf, sizeof(buf), 0);

		if (j < 0) {
			ERR(errno, "Failed receiving file %s from client", path);
			result = 1;
			break;
		}
		if (j == 0)
			break;

		fwrite(buf, 1, j, fp);
	}

	if (result) {
		send_msg(ctrl->sd, "426 TCP connection was established but then broken\r\n");
	} else {
		LOG("User %s at %s uploaded file %s", ctrl->name, ctrl->hisaddr, _path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

//
void handle_DELE(ctx_t *ctrl __attribute__((unused)), char *name __attribute__((unused)))
{

}

//
void handle_MKD(ctx_t *ctrl __attribute__((unused)))
{

}

//
void handle_RMD(ctx_t *ctrl __attribute__((unused)))
{

}

//
void handle_SIZE(ctx_t *ctrl, char *file)
{
	char path[300];
	struct stat st;

	compose_path(ctrl, file, path, sizeof(path));

	if (-1 == stat(path, &st)) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	snprintf(path, sizeof(path), "213 %td\r\n", st.st_size);
	send_msg(ctrl->sd, path);
}

//
void handle_RNFR(ctx_t *ctrl __attribute__((unused)))
{

}

//
void handle_RNTO(ctx_t *ctrl __attribute__((unused)))
{

}

//
void handle_QUIT(ctx_t *ctrl)
{
	send_msg(ctrl->sd, "221 Goodbye.\r\n");
}

//
void handle_CLNT(ctx_t *ctrl)
{
	send_msg(ctrl->sd, "200 CLNT\r\n");
}

//
void handle_OPTS(ctx_t *ctrl)
{
	send_msg(ctrl->sd, "200 UTF8 OPTS ON\r\n");
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
