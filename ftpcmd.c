/* FTP engine
 *
 * Copyright (c) 2014-2016  Joachim Nilsson <troglobit@gmail.com>
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
#include <arpa/ftp.h>

typedef struct {
	char *command;
	void (*cb)(ctrl_t *ctr, char *arg);
} ftp_cmd_t;

static ftp_cmd_t supported[];


static int send_msg(int sd, char *msg)
{
	int n = 0;
	int l;

	if (!msg) {
	err:
		ERR(EINVAL, "Missing argument to send_msg()");
		return 1;
	}

	l = strlen(msg);
	if (l <= 0)
		goto err;

	while (n < l) {
		int result = send(sd, msg + n, l, 0);

		if (result < 0) {
			perror("Failed sending message to client");
			return 1;
		}

		n += result;
	}

	DBG("Sent: %s", msg);

	return 0;
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

		break;
	}

	if (!bytes) {
		INFO("Client disconnected.");
		return 1;
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

	DBG("Recv: %s %s", *cmd, *argument ?: "");

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
		char client_ip[100];

		ctrl->data_sd = accept(ctrl->data_listen_sd, (struct sockaddr *)&sin, &len);
		if (-1 == ctrl->data_sd) {
			perror("Failed accepting connection from client");
			return -1;
		}

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

	return 0;
}

static void close_data_connection(ctrl_t *ctrl)
{
	/* PASV server listening socket */
	if (ctrl->data_listen_sd > 0) {
		shutdown(ctrl->data_listen_sd, SHUT_RDWR);
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
	}

	/* PASV client socket */
	if (ctrl->data_sd > 0) {
		shutdown(ctrl->data_sd, SHUT_RDWR);
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

static void handle_USER(ctrl_t *ctrl, char *name)
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

static void handle_PASS(ctrl_t *ctrl, char *pass)
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

static void handle_SYST(ctrl_t *ctrl, char *UNUSED(arg))
{
	char system[] = "215 UNIX Type: L8\r\n";

	send_msg(ctrl->sd, system);
}

static void handle_TYPE(ctrl_t *ctrl, char *argument)
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

static void handle_PWD(ctrl_t *ctrl, char *UNUSED(arg))
{
	char buf[300];

	snprintf(buf, sizeof(buf), "257 \"%s\"\r\n", ctrl->cwd);
	send_msg(ctrl->sd, buf);
}

static void handle_CWD(ctrl_t *ctrl, char *path)
{
	char *dir = compose_path(ctrl, path);

	if (chdir(dir)) {
		send_msg(ctrl->sd, "550 No such directory.\r\n");
		return;
	}

	if (!chrooted)
		strlcpy(ctrl->cwd, dir + strlen(home), sizeof(ctrl->cwd));
	else
		strlcpy(ctrl->cwd, dir, sizeof(ctrl->cwd));

	send_msg(ctrl->sd, "250 OK\r\n");
}

static void handle_PORT(ctrl_t *ctrl, char *str)
{
	int a, b, c, d, e, f;
	char addr[INET_ADDRSTRLEN];
	struct sockaddr_in sin;

	if (ctrl->data_sd > 0) {
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
	}

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

static void list(ctrl_t *ctrl, char *path, int nlst, char *buf, size_t bufsz, int dirs)
{
	int i, num;
	char *pos = buf;
	size_t len = bufsz - 1;
	struct dirent **d;

	memset(buf, 0, bufsz);

	DBG("Reading directory %s ...", path);
	num = scandir(path, &d, NULL, alphasort);
	for (i = 0; i < num; i++) {
		char *name;
		struct stat st;
		struct dirent *entry = d[i];

		name = entry->d_name;
		DBG("Found directory entry %s", name);
		if (!strcmp(name, ".") || !strcmp(name, ".."))
			goto next;

		path = compose_path(ctrl, name);
		if (stat(path, &st)) {
			LOGIT(LOG_INFO, errno, "Failed reading status for %s", path);
			goto next;
		}

		if (dirs && !S_ISDIR(st.st_mode))
			goto next;
		if (!dirs && S_ISDIR(st.st_mode))
			goto next;

		if (nlst)
			snprintf(pos, len, "%s", name);
		else
			snprintf(pos, len,
				 "%s 1 %5d %5d %12" PRIu64 " %s %s",
				 mode_to_str(st.st_mode),
				 0, 0, (uint64_t)st.st_size,
				 time_to_str(st.st_mtime), name);

		if (ctrl->type == TYPE_A)
			strlcat(pos, "\r\n", len);
		else
			strlcat(pos, "\n", len);

		DBG("LIST %s", pos);
		len -= strlen(pos);
		pos += strlen(pos);

		if (len < 80) {
			if (strlen(buf))
				send_msg(ctrl->data_sd, buf);
			memset(buf, 0, bufsz);
			pos = buf;
			len = bufsz - 1;
		}

	next:
		free(entry);
	}

	if (strlen(buf))
		send_msg(ctrl->data_sd, buf);

	free(d);
}

static void do_list(ctrl_t *ctrl, char *arg, int nlst)
{
	char *buf, *path;
	size_t sz = BUFFER_SIZE * sizeof(char);

	if (string_valid(arg)) {
		char *ptr = arg;

		/* Check if client sends ls arguments ... */
		while (*ptr) {
			if (*ptr == ' ')
				ptr++;
			if (string_match(ptr, "-l"))
				ptr += 2;
			else
				break;
		}

		arg = ptr;
	}

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

	INFO("Established TCP socket for data communication.");
	send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");

	/* Call list() twice to list directories first, then regular files */
	path = compose_path(ctrl, arg);
	list(ctrl, path, nlst, buf, sz, 1);
	path = compose_path(ctrl, arg);
	list(ctrl, path, nlst, buf, sz, 0);

	free(buf);
	close_data_connection(ctrl);
	send_msg(ctrl->sd, "226 Transfer complete.\r\n");
}

static void handle_LIST(ctrl_t *ctrl, char *arg)
{
	do_list(ctrl, arg, 0);
}

static void handle_NLST(ctrl_t *ctrl, char *arg)
{
	do_list(ctrl, arg, 1);
}

/* XXX: Audit this, does it really work with multiple interfaces? */
static void handle_PASV(ctrl_t *ctrl, char *UNUSED(arg))
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

	INFO("Data server port estabished.  Waiting for client connnect ...");
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

	/* Convert server IP address and port to comma separated list */
	msg = strdup(ctrl->serveraddr);
	if (!msg) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		exit(1);
	}
	p = msg;
	while ((p = strchr(p, '.')))
		*p++ = ',';

	port = ntohs(data.sin_port);
	snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%s,%d,%d)\r\n",
		 msg, port / 256, port % 256);
	send_msg(ctrl->sd, buf);

	free(msg);
}

static void handle_RETR(ctrl_t *ctrl, char *file)
{
	int result = 0;
	FILE *fp = NULL;
	char *buf;
	char *path = compose_path(ctrl, file);
	size_t len = BUFFER_SIZE * sizeof(char);
	struct stat st;

	if (stat(path, &st) || !S_ISREG(st.st_mode)) {
		send_msg(ctrl->sd, "550 Not a regular file.\r\n");
		return;
	}

	buf = malloc(len);
	if (!buf) {
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		if (errno != ENOENT)
			ERR(errno, "%s: Failed opening file %s for RETR", ctrl->clientaddr, path);
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
		INFO("User %s from %s downloaded file %s", ctrl->name, ctrl->clientaddr, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

static void handle_MDTM(ctrl_t *ctrl, char *file)
{
	char buf[80];
	char *path = compose_path(ctrl, file);
	struct tm *tm;
	struct stat st;

	if (stat(path, &st) || !S_ISREG(st.st_mode)) {
		send_msg(ctrl->sd, "550 Not a regular file.\r\n");
		return;
	}

	tm = gmtime(&st.st_mtime);
	strftime(buf, sizeof(buf), "213 %Y%m%d%H%M%S\r\n", tm);
	send_msg(ctrl->sd, buf);
}

static void handle_STOR(ctrl_t *ctrl, char *file)
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
		INFO("User %s at %s uploaded file %s/%s", ctrl->name, ctrl->clientaddr, ctrl->cwd, path);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
	}

	close_data_connection(ctrl);
	fclose(fp);
	free(buf);
}

#if 0
static void handle_DELE(ctrl_t *UNUSED(ctrl), char *UNUSED(file))
{
}

static void handle_MKD(ctrl_t *UNUSED(ctrl), char *UNUSED(arg))
{
}

static void handle_RMD(ctrl_t *UNUSED(ctrl), char *UNUSED(arg))
{
}
#endif

static size_t num_nl(char *file)
{
	FILE *fp;
	char buf[80];
	size_t len, num = 0;

	fp = fopen(file, "r");
	if (!fp)
		return 0;

	do {
		char *ptr = buf;

		len = fread(buf, sizeof(char), sizeof(buf) - 1, fp);
		if (len > 0) {
			buf[len] = 0;
			while ((ptr = strchr(ptr, '\n'))) {
				ptr++;
				num++;
			}
		}
	} while (len > 0);
	fclose(fp);

	return num;
}

static void handle_SIZE(ctrl_t *ctrl, char *file)
{
	char *path = compose_path(ctrl, file);
	size_t extralen = 0;
	struct stat st;

	DBG("SIZE %s", path);

	if (-1 == stat(path, &st)) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	if (ctrl->type == TYPE_A)
		extralen = num_nl(file);

	sprintf(path, "213 %"  PRIu64 "\r\n", (uint64_t)(st.st_size + extralen));
	send_msg(ctrl->sd, path);
}

/* No operation - used as session keepalive by clients. */
static void handle_NOOP(ctrl_t *ctrl, char *UNUSED(arg))
{
	send_msg(ctrl->sd, "200 NOOP OK.\r\n");
}

#if 0
static void handle_RNFR(ctrl_t *UNUSED(ctrl), char *UNUSED(arg))
{
}

static void handle_RNTO(ctrl_t *UNUSED(ctrl), char *UNUSED(arg))
{
}
#endif

static void handle_QUIT(ctrl_t *ctrl, char *UNUSED(arg))
{
	send_msg(ctrl->sd, "221 Goodbye.\r\n");
	uev_exit(ctrl->ctx);
}

static void handle_CLNT(ctrl_t *ctrl, char *UNUSED(arg))
{
	send_msg(ctrl->sd, "200 CLNT\r\n");
}

static void handle_OPTS(ctrl_t *ctrl, char *UNUSED(arg))
{
	send_msg(ctrl->sd, "200 UTF8 OPTS ON\r\n");
}

static void handle_HELP(ctrl_t *ctrl, char *arg)
{
	int i = 0;
	char buf[80];
	ftp_cmd_t *cmd;

	if (string_valid(arg) && !string_compare(arg, "SITE")) {
		send_msg(ctrl->sd, "500 command HELP does not take any arguments on this server.\r\n");
		return;
	}

	snprintf(ctrl->buf, ctrl->bufsz, "214-The following commands are recognized.");
	for (cmd = &supported[0]; cmd->command; cmd++, i++) {
		if (i % 14 == 0)
			strlcat(ctrl->buf, "\r\n", ctrl->bufsz);
		snprintf(buf, sizeof(buf), " %s", cmd->command);
		strlcat(ctrl->buf, buf, ctrl->bufsz);
	}
	snprintf(buf, sizeof(buf), "\r\n214 Help OK.\r\n");
	strlcat(ctrl->buf, buf, ctrl->bufsz);

	send_msg(ctrl->sd, ctrl->buf);
}

static void handle_FEAT(ctrl_t *ctrl, char *UNUSED(arg))
{
	snprintf(ctrl->buf, ctrl->bufsz, "211-Features:\r\n"
		" PASV\r\n"
		" SIZE\r\n"
		" UTF8\r\n"
		"211 End\r\n");
	send_msg(ctrl->sd, ctrl->buf);
}

static void handle_UNKNOWN(ctrl_t *ctrl, char *command)
{
	char buf[128];

	snprintf(buf, sizeof(buf), "500 command '%s' not recognized by server.\r\n", command);
	send_msg(ctrl->sd, buf);
}

#define COMMAND(NAME) { #NAME, handle_ ## NAME }

static ftp_cmd_t supported[] = {
	COMMAND(USER),
	COMMAND(PASS),
	COMMAND(SYST),
	COMMAND(TYPE),
	COMMAND(PORT),
	COMMAND(RETR),
	COMMAND(MDTM),
	COMMAND(PASV),
	COMMAND(QUIT),
	COMMAND(LIST),
	COMMAND(NLST),
	COMMAND(CLNT),
	COMMAND(OPTS),
	COMMAND(PWD),
	COMMAND(STOR),
	COMMAND(CWD),
	COMMAND(SIZE),
	COMMAND(NOOP),
	COMMAND(HELP),
	COMMAND(FEAT),
	{ NULL, NULL }
};

static void child_exit(uev_t *w, void *UNUSED(arg), int UNUSED(events))
{
	uev_exit(w->ctx);
}

static void read_client_command(uev_t *w, void *arg, int UNUSED(events))
{
	char *command, *argument;
	ctrl_t *ctrl = (ctrl_t *)arg;
	ftp_cmd_t *cmd;

	/* Reset inactivity timer. */
	uev_timer_set(&ctrl->timeout_watcher, INACTIVITY_TIMER, 0);

	if (recv_msg(w->fd, ctrl->buf, ctrl->bufsz, &command, &argument)) {
		uev_exit(ctrl->ctx);
		return;
	}

	if (!string_valid(command))
		return;

	for (cmd = &supported[0]; cmd->command; cmd++) {
		if (string_compare(command, cmd->command)) {
			cmd->cb(ctrl, argument);
			return;
		}
	}

	handle_UNKNOWN(ctrl, command);
}

static void ftp_command(ctrl_t *ctrl)
{
	uev_t sigterm_watcher;

	ctrl->bufsz = BUFFER_SIZE * sizeof(char);
	ctrl->buf   = malloc(ctrl->bufsz);
	if (!ctrl->buf) {
                WARN(errno, "FTP session failed allocating buffer");
                exit(1);
	}

	snprintf(ctrl->buf, ctrl->bufsz, "220 %s (%s) ready.\r\n", prognm, VERSION);
	send_msg(ctrl->sd, ctrl->buf);

	uev_signal_init(ctrl->ctx, &sigterm_watcher, child_exit, NULL, SIGTERM);
	uev_io_init(ctrl->ctx, &ctrl->io_watcher, read_client_command, ctrl, ctrl->sd, UEV_READ);
	uev_run(ctrl->ctx, 0);
}

int ftp_session(uev_ctx_t *ctx, int sd)
{
	int pid = 0;
	ctrl_t *ctrl;
	socklen_t len;

	ctrl = new_session(ctx, sd, &pid);
	if (!ctrl) {
		if (pid < 0) {
			shutdown(sd, SHUT_RDWR);
			close(sd);
		}

		return pid;
	}

	len = sizeof(ctrl->server_sa);
	if (-1 == getsockname(sd, (struct sockaddr *)&ctrl->server_sa, &len)) {
		perror("Cannot determine our address");
		goto fail;
	}
	convert_address(&ctrl->server_sa, ctrl->serveraddr, sizeof(ctrl->serveraddr));

	len = sizeof(ctrl->client_sa);
	if (-1 == getpeername(sd, (struct sockaddr *)&ctrl->client_sa, &len)) {
		perror("Cannot determine client address");
		goto fail;
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

	exit(del_session(ctrl, 1));
fail:
	free(ctrl);
	shutdown(sd, SHUT_RDWR);
	close(sd);

	return -1;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
