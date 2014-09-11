/* FTP engine
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

#include "ftpcmd.h"


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
static int recv_msg(int sd, char *msg, size_t len, char **cmd, char **argument)
{
	char *ptr;
	ssize_t bytes;

	/* Clear for every new command. */
	memset(msg, 0, len);

	/* Save one byte (-1) for NUL termination */
	while ((bytes = recv(sd, msg, len - 1, 0))) {
		if (bytes < 0) {
			if (EINTR == errno)
				return 1;

			ERR(errno, "Failed reading client command");
			return 1;
		}

		if (!bytes) {
			show_log("Client disconnected.");
			return 1;
		}

		break;
	}

	/* NUL terminate for strpbrk() */
	msg[bytes] = 0;

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

	return 0;
}

static int open_data_connection(ctrl_t *ctrl)
{
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in sin;

	/* Previous PORT command from client */
	if (ctrl->data_address[0]) {
		ctrl->data_sd = socket(AF_INET, SOCK_STREAM, 0);
		if (-1 == ctrl->data_sd) {
			perror("Failed creating data socket");
			return -1;
		}

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(ctrl->data_port);
		inet_aton(ctrl->data_address, &(sin.sin_addr));

		if (connect(ctrl->data_sd, (struct sockaddr *)&sin, len) == -1) {
			perror("Failed connecting data socket to client");
			close(ctrl->data_sd);
			ctrl->data_sd = -1;

			return -1;
		}

		DBG("Connected successfully to client's previously requested address:PORT %s:%d", ctrl->data_address, ctrl->data_port);
		return 0;
	}

	/* Previous PASV command, accept connect from client */
	if (ctrl->data_listen_sd > 0) {
		ctrl->data_sd = accept(ctrl->data_listen_sd, (struct sockaddr *)&sin, &len);
		if (-1 == ctrl->data_sd) {
			perror("Failed accepting connection from client");
			return -1;
		} else {
			char client_ip[100];

			len = sizeof(struct sockaddr);
			if (-1 == getpeername(ctrl->data_sd, (struct sockaddr *)&sin, &len)) {
				perror("Cannot determine client address");
				close(ctrl->data_sd);
				ctrl->data_sd = -1;
				return -1;
			}
			inet_ntop(AF_INET, &(sin.sin_addr), client_ip, INET_ADDRSTRLEN);
			DBG("Client PASV data connection from %s", client_ip);
		}
	}

	return 0;
}

static void close_data_connection(ctrl_t *ctrl)
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

static int check_user_pass(ctrl_t *ctrl)
{
	if (!ctrl->name[0])
		return -1;

	if (!strcmp("anonymous", ctrl->name))
		return 1;

	return 0;
}

void handle_USER(ctrl_t *ctrl, char *name)
{
	if (ctrl->name[0]) {
		ctrl->name[0] = 0;
		ctrl->pass[0] = 0;
	}

	if (name) {
		strlcpy(ctrl->name, name, sizeof(ctrl->name));
		if (check_user_pass(ctrl) == 1) {
			INFO("Guest logged in from %s", ctrl->clientaddr);
			send_msg(ctrl->sd, "230 Guest login OK, access restrictions apply.\r\n");
		} else {
			send_msg(ctrl->sd, "331 Login OK, please enter password.\r\n");
		}
	} else {
		send_msg(ctrl->sd, "530 You must input your name.\r\n");
	}
}

void handle_PASS(ctrl_t *ctrl, char *pass)
{
	if (!ctrl->name[0]) {
		send_msg(ctrl->sd, "503 No username given.\r\n");
		return;
	}

	strlcpy(ctrl->pass, pass, sizeof(ctrl->pass));
	if (check_user_pass(ctrl) < 0) {
		LOG("User %s from %s, invalid password!", ctrl->name, ctrl->clientaddr);
		send_msg(ctrl->sd, "530 username or password is unacceptable\r\n");
		return;
	}

	INFO("User %s login from %s", ctrl->name, ctrl->clientaddr);
	send_msg(ctrl->sd, "230 Guest login OK, access restrictions apply.\r\n");
}

void handle_SYST(ctrl_t *ctrl)
{
	char system[] = "215 UNIX Type: L8\r\n";

	send_msg(ctrl->sd, system);
}

void handle_TYPE(ctrl_t *ctrl, char *argument)
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

void handle_PWD(ctrl_t *ctrl)
{
	char buf[300];

	snprintf(buf, sizeof(buf), "257 \"%s\"\r\n", ctrl->cwd);
	send_msg(ctrl->sd, buf);
}

void handle_CWD(ctrl_t *ctrl, char *path)
{
	char *dir = compose_path(ctrl, path);

	if (chdir(dir)) {
		WARN(errno, "Client from %s tried to change to non existing dir %s", ctrl->clientaddr, dir);
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	if (!chrooted)
		strlcpy(ctrl->cwd, dir + strlen(home), sizeof(ctrl->cwd));
	else
		strlcpy(ctrl->cwd, dir, sizeof(ctrl->cwd));
	DBG("Saved new CWD: %s", ctrl->cwd);

	send_msg(ctrl->sd, "250 OK\r\n");
}

void handle_PORT(ctrl_t *ctrl, char *str)
{
	int a, b, c, d, e, f;
	char addr[INET_ADDRSTRLEN];
	struct sockaddr_in sin;

	if (ctrl->data_sd > 0)
		close(ctrl->data_sd);

	/* Convert PORT command's argument to IP address + port */
	sscanf(str, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &e, &f);
	sprintf(addr, "%d.%d.%d.%d", a, b, c, d);

	/* Check IPv4 address using inet_aton(), throw away converted result */
	if (!inet_aton(addr, &(sin.sin_addr))) {
		ERR(0, "Invalid address '%s' given to PORT command", addr);
		send_msg(ctrl->sd, "500 Illegal PORT command.\r\n");
		return;
	}

	strlcpy(ctrl->data_address, addr, sizeof(ctrl->data_address));
	ctrl->data_port = e * 256 + f;

	DBG("Client PORT command accepted for %s:%d", ctrl->data_address, ctrl->data_port);
	send_msg(ctrl->sd, "200 PORT command successful.\r\n");
}

static char *mode_to_str(mode_t m)
{
	static char str[11];

	snprintf(str, sizeof(str), "%c%c%c%c%c%c%c%c%c%c",
		 S_ISDIR(m)  ? 'd' : '-',
		 m & S_IRUSR ? 'r' : '-',
		 m & S_IWUSR ? 'w' : '-',
		 m & S_IXUSR ? 'x' : '-',
		 m & S_IRGRP ? 'r' : '-',
		 m & S_IWGRP ? 'w' : '-',
		 m & S_IXGRP ? 'x' : '-',
		 m & S_IROTH ? 'r' : '-',
		 m & S_IWOTH ? 'w' : '-',
		 m & S_IXOTH ? 'x' : '-');

	return str;
}

static char *time_to_str(time_t mtime)
{
	struct tm *t = localtime(&mtime);
	static char str[20];

	setlocale(LC_TIME, "C");
	strftime(str, sizeof(str), "%b %e %H:%M", t);

	return str;
}

void handle_LIST(ctrl_t *ctrl)
{
	DIR *dir;
	char *buf;
	char *path = compose_path(ctrl, NULL);
	size_t sz = BUFFER_SIZE * sizeof(char);

	buf = malloc(sz);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	if (open_data_connection(ctrl)) {
		free(buf);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		return;
	}

	show_log("Established TCP socket for data communication.");
	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");

	dir = opendir(path);
	while (dir) {
		char *pos = buf;
		size_t len = sz;
		struct dirent *entry;

		DBG("Reading directory %s ...", path);
		while ((entry = readdir(dir)) && len > 80) {
			struct stat st;
			char *name = entry->d_name;

			DBG("Found directory entry %s", name);
			if (!strcmp(name, ".") || !strcmp(name, ".."))
				continue;

			path = compose_path(ctrl, name);
			if (stat(path, &st)) {
				ERR(errno, "Failed reading status for %s", path);
				continue;
			}

			snprintf(pos, len, "%s 1 %5d %5d %12"  PRIu64 " %s %s%s\n",
				 mode_to_str(st.st_mode),
				 0, 0, (uint64_t)st.st_size,
				 time_to_str(st.st_mtime),
				 name, ctrl->type == TYPE_A ? "\r" : "");

			DBG("LIST %s", pos);
			len -= strlen(pos);
			pos += strlen(pos);
		}

		send_msg(ctrl->data_sd, buf);
		if (entry)
			continue;
		closedir(dir);
		break;
	}

	free(buf);
	close_data_connection(ctrl);
	send_msg(ctrl->sd, "226 Transfer complete.\r\n");
}

/* XXX: Audit this, does it really work with multiple interfaces? */
void handle_PASV(ctrl_t *ctrl)
{
	int port;
	char *msg, *p, buf[200];
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
		ERR(errno, "Failed opening data server socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = inet_addr(ctrl->serveraddr);
	server.sin_port        = htons(0);
	if (bind(ctrl->data_listen_sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
		ERR(errno, "Failed binding to client socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return;
	}

	show_log("Data server port estabished.  Waiting for client connnect ...");
	if (listen(ctrl->data_listen_sd, 1) < 0) {
		ERR(errno, "Client data connection failure");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return;
	}

	memset(&data, 0, sizeof(data));
	if (-1 == getsockname(ctrl->data_listen_sd, (struct sockaddr *)&data, &len)) {
		ERR(errno, "Cannot determine our address, need it if client should connect to us");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return;
	}

	port = ntohs(data.sin_port);
	snprintf(buf, sizeof(buf), "Port %d\n", port);
	show_log(buf);


	/* Convert server IP address and port to comma separated list */
	msg = strdup(ctrl->serveraddr);
	if (!msg) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		exit(1);
	}
	p = msg;
	while ((p = strchr(p, '.')))
		*p++ = ',';

	snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%s,%d,%d)\r\n",
		 msg, port / 256, port % 256);
	send_msg(ctrl->sd, buf);

	free(msg);
}

void handle_RETR(ctrl_t *ctrl, char *file)
{
	int result = 0;
	FILE *fp = NULL;
	char *buf;
	char *path = compose_path(ctrl, file);
	size_t len = BUFFER_SIZE * sizeof(char);

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		ERR(errno, "Failed opening file %s for RETR", path);
		free(buf);
		send_msg(ctrl->sd, "451 Trouble to RETR file.\r\n");
		return;
	}

	if (open_data_connection(ctrl)) {
		fclose(fp);
		free(buf);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		return;
	}

	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");

	while (!feof(fp) && !result) {
		int n = fread(buf, sizeof(char), len, fp);
		int j = 0;

		while (j < n) {
			ssize_t bytes = send(ctrl->data_sd, buf + j, n - j, 0);

			if (-1 == bytes) {
				ERR(errno, "Failed sending file %s to client", path);
				result = 1;
				break;
			}
			j += bytes;
		}
	}

	if (result) {
		send_msg(ctrl->sd, "426 TCP connection was established but then broken!\r\n");
	} else {
		LOG("User %s from %s downloaded file %s", ctrl->name, ctrl->clientaddr, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

void handle_STOR(ctrl_t *ctrl, char *file)
{
	int result = 0;
	FILE *fp = NULL;
	char *buf, *path = compose_path(ctrl, file);
	size_t len = BUFFER_SIZE * sizeof(char);

	DBG("STOR %s", path);

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	fp = fopen(path, "wb");
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
		int j = recv(ctrl->data_sd, buf, len, 0);

		if (j < 0) {
			ERR(errno, "Failed receiving file %s/%s from client", ctrl->cwd, path);
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
		LOG("User %s at %s uploaded file %s/%s", ctrl->name, ctrl->clientaddr, ctrl->cwd, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

void handle_DELE(ctrl_t *UNUSED(ctrl), char *UNUSED(file))
{

}

//
void handle_MKD(ctrl_t *UNUSED(ctrl))
{

}

//
void handle_RMD(ctrl_t *UNUSED(ctrl))
{

}

//
void handle_SIZE(ctrl_t *ctrl, char *file)
{
	char *path = compose_path(ctrl, file);
	struct stat st;

	DBG("SIZE %s", path);

	if (-1 == stat(path, &st)) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	sprintf(path, "213 %"  PRIu64 "\r\n", (uint64_t)st.st_size);
	send_msg(ctrl->sd, path);
}

//
void handle_RNFR(ctrl_t *UNUSED(ctrl))
{

}

//
void handle_RNTO(ctrl_t *UNUSED(ctrl))
{

}

//
void handle_QUIT(ctrl_t *ctrl)
{
	send_msg(ctrl->sd, "221 Goodbye.\r\n");
}

//
void handle_CLNT(ctrl_t *ctrl)
{
	send_msg(ctrl->sd, "200 CLNT\r\n");
}

//
void handle_OPTS(ctrl_t *ctrl)
{
	send_msg(ctrl->sd, "200 UTF8 OPTS ON\r\n");
}


void ftp_command(ctrl_t *ctrl)
{
	size_t len = BUFFER_SIZE * sizeof(char);
	char *buffer;
	char *cmd;
	char *argument;
	struct sigaction sa;

	buffer = malloc(len);
	if (!buffer) {
                perror("Out of memory");
                exit(1);
	}

	snprintf(buffer, len, "220 %s (%s) ready.\r\n", __progname, VERSION);
	send_msg(ctrl->sd, buffer);

	SETSIG(sa, SIGALRM, sigalrm_handler, SA_RESTART);

	while (1) {
		alarm(INACTIVITY_TIMER);

		if (recv_msg(ctrl->sd, buffer, len, &cmd, &argument))
			break;

		alarm(0);

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
			handle_RETR(ctrl, argument);
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

			snprintf(buf, sizeof(buf), "500 %s command not recognized by server\r\n", cmd);
			send_msg(ctrl->sd, buf);
		}
	}

	free(buffer);
	close(ctrl->sd);
}

int ftp_session(int sd)
{
	int pid = 0;
	ctrl_t *ctrl;
	socklen_t len;

	ctrl = new_session(sd, &pid);
	if (!ctrl) {
		if (-1 == pid)
			return -1;
		return 0;
	}

	len = sizeof(ctrl->server_sa);
	if (-1 == getsockname(sd, (struct sockaddr *)&ctrl->server_sa, &len)) {
		perror("Cannot determine our address");
		return -1;
	}
	convert_address(&ctrl->server_sa, ctrl->serveraddr, sizeof(ctrl->serveraddr));

	len = sizeof(ctrl->client_sa);
	if (-1 == getpeername(sd, (struct sockaddr *)&ctrl->client_sa, &len)) {
		perror("Cannot determine client address");
		return -1;
	}
	convert_address(&ctrl->client_sa, ctrl->clientaddr, sizeof(ctrl->clientaddr));

	ctrl->type = TYPE_A;
	ctrl->data_listen_sd = -1;
	ctrl->data_sd = -1;
	ctrl->name[0] = 0;
	ctrl->pass[0] = 0;
	ctrl->data_address[0] = 0;

	INFO("Client connection from %s", ctrl->clientaddr);
	ftp_command(ctrl);

	DBG("Exiting ...");
	del_session(ctrl);

	exit(0);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
