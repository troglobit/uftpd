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

static int sd       = -1;
static int chrooted = 0;

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
		int client;

		client = accept(sd, NULL, NULL);
		if (client < 0) {
			perror("Failed accepting incoming client connection");
			continue;
		}

		start_session(client);
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
static int recv_msg(int sd, char *msg, size_t len, char **cmd, char **argument)
{
	char *ptr;
	ssize_t bytes;

	/* Clear for every new command. */
	memset(msg, 0, len);

	while ((bytes = recv(sd, msg, len, 0))) {
		if (bytes < 0) {
			if (EINTR == errno)
				continue;

			ERR(errno, "Failed reading client command");
			return 1;
		}

		if (!bytes) {
			show_log("Client disconnected.");
			return 1;
		}

		break;
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

	return 0;
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

/* Check for /some/path/../new/path => /some/new/path */
static void squash_dots(char *path)
{
	char *dots, *ptr;

	while ((dots = strstr(path, "/../"))) {
		/* Walking up to parent attack */
		if (path == dots) {
			memmove(&path[0], &path[3], strlen(&path[3]) + 1);
			continue;
		}

		ptr = strrchr(dots, '/');
		if (ptr) {
			dots += 3;
			memmove(ptr, dots, strlen(dots));
		}
	}
}

static char *compose_path(ctx_t *ctrl, char *path)
{
	static char dir[PATH_MAX];

	strlcpy(dir, ctrl->cwd, sizeof(dir));

	DBG("Compose path from cwd: %s, arg: %s", ctrl->cwd, path);
	if (!path || path[0] != '/') {
		if (path && path[0] != 0) {
			if (dir[strlen(dir) - 1] != '/')
				strlcat(dir, "/", sizeof(dir));
			strlcat(dir, path, sizeof(dir));
		}
	} else {
		strlcpy(dir, path, sizeof(dir));
	}

	squash_dots(dir);

	if (!chrooted) {
		size_t len = strlen(home);

		DBG("Server path from CWD: %s", dir);
		memmove(dir + len, dir, strlen(dir) + 1);
		memcpy(dir, home, len);
		DBG("Resulting non-chroot path: %s", dir);
	}

	return dir;
}

static void handle_command(ctx_t *ctrl)
{
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
		if (recv_msg(ctrl->sd, buffer, len, &cmd, &argument))
			break;

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

static int session(ctx_t *ctrl)
{
	static int privs_dropped = 0;

	/* Chroot to FTP root */
	if (!chrooted && geteuid() == 0) {
		if (chroot(home) || chdir(".")) {
			ERR(errno, "Failed chrooting to FTP root, %s, aborting", home);
			return -1;
		}
		chrooted = 1;
	} else if (!chrooted) {
		if (chdir(home)) {
			WARN(errno, "Failed changing to FTP root, %s, aborting", home);
			return -1;
		}
	}

	/* If ftp user exists and we're running as root we can drop privs */
	if (!privs_dropped && pw && geteuid() == 0) {
		if (setuid(pw->pw_uid) || setgid(pw->pw_gid))
			WARN(errno, "Failed dropping privileges to uid:gid %d:%d",
			     pw->pw_uid, pw->pw_gid);
		else
			privs_dropped = 1;
	}

	handle_command(ctrl);
	stop_session(ctrl);

	return 0;
}

int start_session(int sd)
{
	pid_t pid;
	ctx_t *ctrl;
	socklen_t len;
	struct sockaddr_in host_addr;
	struct sockaddr_in client_addr;

	ctrl = malloc(sizeof(ctx_t));
	if (!ctrl) {
		ERR(errno, "Failed allocating session context");
		return -1;
	}

	/* Find our address */
	len = sizeof(struct sockaddr);
	getsockname(sd, (struct sockaddr *)&host_addr, &len);
	inet_ntop(AF_INET, &(host_addr.sin_addr), ctrl->ouraddr, sizeof(ctrl->ouraddr));

	/* Find peer address */
	len = sizeof(struct sockaddr);
	getpeername(sd, (struct sockaddr *)&client_addr, &len);
	inet_ntop(AF_INET, &(client_addr.sin_addr), ctrl->hisaddr, sizeof(ctrl->hisaddr));

	ctrl->sd = sd;
	strlcpy(ctrl->cwd, "/", sizeof(ctrl->cwd));
	ctrl->type = TYPE_A;
	ctrl->status = 0;
	ctrl->data_listen_sd = -1;
	ctrl->data_sd = -1;
	ctrl->name[0] = 0;
	ctrl->pass[0] = 0;
	ctrl->data_address[0] = 0;

	if (!inetd) {
		pid = fork();
		if (pid) {
			DBG("Forked off client session as PID %d", pid);
			return pid;
		}
	}

	INFO("Client connection from %s", ctrl->hisaddr);
	return session(ctrl);
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
			INFO("Guest logged in from %s", ctrl->hisaddr);
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

	INFO("User %s login from %s", ctrl->name, ctrl->hisaddr);
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

void handle_CWD(ctx_t *ctrl, char *path)
{
	char *dir = compose_path(ctrl, path);

	if (chdir(dir)) {
		WARN(errno, "Client from %s tried to change to non existing dir %s", ctrl->hisaddr, dir);
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
		send_msg(ctrl->sd, "500 Illegal PORT command.\r\n");
		return;
	}

	strlcpy(ctrl->data_address, addr, sizeof(ctrl->data_address));
	ctrl->data_port = e * 256 + f;

	DBG("Client PORT command accepted for %s:%s", ctrl->data_address, ctrl->data_port);
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

void handle_LIST(ctx_t *ctrl)
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
		char *pos;
		size_t len = sz;
		struct dirent *entry;

		pos = buf;
		while ((entry = readdir(dir))) {
			struct stat st;
			char *name = entry->d_name;

			if (!strcmp(name, ".") || !strcmp(name, ".."))
				continue;

			path = compose_path(ctrl, name);
			if (stat(path, &st)) {
				ERR(errno, "Failed reading status for %s", path);
				continue;
			}

			snprintf(pos, len, "%s 1 %5d %5d %12td %s %s%s\n",
				 mode_to_str(st.st_mode),
				 0, 0, st.st_size,
				 time_to_str(st.st_mtime),
				 name, ctrl->type == TYPE_A ? "\r" : "");

			DBG("LIST %s", pos);
			pos = pos + strlen(pos);
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
		ERR(errno, "Failed opening data server socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = inet_addr(ctrl->ouraddr);
	server.sin_port        = htons(0);
	if (bind(ctrl->data_listen_sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) {
		ERR(errno, "Failed binding to client socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	show_log("Data server port estabished.  Waiting for client connnect ...");
	if (listen(ctrl->data_listen_sd, 1) < 0) {
		ERR(errno, "Client data connection failure");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
	}

	getsockname(ctrl->data_listen_sd, (struct sockaddr *)&data, &len);
	port = ntohs(data.sin_port);
	/* XXX: s/ctrl->ouraddr/data.sin_addr.saddr? */
	msg = _transfer_ip_port_str(ctrl->ouraddr, port);
	if (!msg) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		exit(1);
	}

	snprintf(buf, sizeof(buf), "Port %d\n", port);
	show_log(buf);

	snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%s)\r\n", msg);
	send_msg(ctrl->sd, buf);

	free(msg);
}

void handle_RETR(ctx_t *ctrl, char *file)
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
		LOG("User %s from %s downloaded file %s", ctrl->name, ctrl->hisaddr, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

void handle_STOR(ctx_t *ctrl, char *file)
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
		int j = recv(ctrl->data_sd, buf, sizeof(buf), 0);

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
		LOG("User %s at %s uploaded file %s/%s", ctrl->name, ctrl->hisaddr, ctrl->cwd, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

void handle_DELE(ctx_t *ctrl __attribute__((unused)), char *file __attribute__((unused)))
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
	char *path = compose_path(ctrl, file);
	struct stat st;

	DBG("SIZE %s", path);

	if (-1 == stat(path, &st)) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	sprintf(path, "213 %td\r\n", st.st_size);
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
