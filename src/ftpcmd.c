/* FTP engine
 *
 * Copyright (c) 2014-2019  Joachim Nilsson <troglobit@gmail.com>
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
#include <ctype.h>
#include <arpa/ftp.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

typedef struct {
	char *command;
	void (*cb)(ctrl_t *ctr, char *arg);
} ftp_cmd_t;

static ftp_cmd_t supported[];

static void do_PORT(ctrl_t *ctrl, int pending);
static void do_LIST(uev_t *w, void *arg, int events);
static void do_RETR(uev_t *w, void *arg, int events);
static void do_STOR(uev_t *w, void *arg, int events);

static int is_cont(char *msg)
{
	char *ptr;

	ptr = strchr(msg, '\r');
	if (ptr) {
		ptr++;
		if (strchr(ptr, '\r'))
			return 1;
	}

	return 0;
}

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
			ERR(errno, "Failed sending message to client");
			return 1;
		}

		n += result;
	}

	DBG("Sent: %s%s", is_cont(msg) ? "\n" : "", msg);

	return 0;
}

/*
 * Receive message from client, split into command and argument
 */
static int recv_msg(int sd, char *msg, size_t len, char **cmd, char **argument)
{
	char *ptr;
	ssize_t bytes;
	uint8_t *raw = (uint8_t *)msg;

	/* Clear for every new command. */
	memset(msg, 0, len);

	/* Save one byte (-1) for NUL termination */
	bytes = recv(sd, msg, len - 1, 0);
	if (bytes < 0) {
		if (EINTR == errno)
			return 1;

		if (ECONNRESET == errno)
			DBG("Connection reset by client.");
		else
			ERR(errno, "Failed reading client command");
		return 1;
	}

	if (!bytes) {
		INFO("Client disconnected.");
		return 1;
	}

	if (raw[0] == 0xff) {
		char tmp[4];
		char buf[20] = { 0 };
		int i;

		i = recv(sd, &msg[bytes], len - bytes - 1, MSG_OOB | MSG_DONTWAIT);
		if (i > 0)
			bytes += i;

		for (i = 0; i < bytes; i++) {
			snprintf(tmp, sizeof(tmp), "%2X%s", raw[i], i + 1 < bytes ? " " : "");
			strlcat(buf, tmp, sizeof(buf));
		}

		strlcpy(msg, buf, len);
		*cmd      = msg;
		*argument = NULL;

		DBG("Recv: [%s], %zd bytes", msg, bytes);

		return 0;
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

	/* Convert command to std ftp upper case, issue #18 */
	for (ptr = msg; *ptr; ++ptr) *ptr = toupper(*ptr);

	DBG("Recv: %s %s", *cmd, *argument ?: "");

	return 0;
}

static int open_data_connection(ctrl_t *ctrl)
{
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in sin;

	/* Previous PORT command from client */
	if (ctrl->data_address[0]) {
		int rc;

		ctrl->data_sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
		if (-1 == ctrl->data_sd) {
			ERR(errno, "Failed creating data socket");
			return -1;
		}

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(ctrl->data_port);
		inet_aton(ctrl->data_address, &(sin.sin_addr));

		rc = connect(ctrl->data_sd, (struct sockaddr *)&sin, len);
		if (rc == -1 && EINPROGRESS != errno) {
			ERR(errno, "Failed connecting data socket to client");
			close(ctrl->data_sd);
			ctrl->data_sd = -1;

			return -1;
		}

		DBG("Connected successfully to client's previously requested address:PORT %s:%d",
		    ctrl->data_address, ctrl->data_port);
		return 0;
	}

	/* Previous PASV command, accept connect from client */
	if (ctrl->data_listen_sd > 0) {
		const int const_int_1 = 1;
		int retries = 3;
		char client_ip[100];

	retry:
		ctrl->data_sd = accept(ctrl->data_listen_sd, (struct sockaddr *)&sin, &len);
		if (-1 == ctrl->data_sd) {
			if (EAGAIN == errno && --retries) {
				sleep(1);
				goto retry;
			}

			ERR(errno, "Failed accepting connection from client");
			return -1;
		}

		setsockopt(ctrl->data_sd, SOL_SOCKET, SO_KEEPALIVE, &const_int_1, sizeof(const_int_1));
		set_nonblock(ctrl->data_sd);

		inet_ntop(AF_INET, &(sin.sin_addr), client_ip, INET_ADDRSTRLEN);
		DBG("Client PASV data connection from %s:%d", client_ip, ntohs(sin.sin_port));

		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
	}

	return 0;
}

static int close_data_connection(ctrl_t *ctrl)
{
	int ret = 0;

	DBG("Closing data connection ...");

	/* PASV server listening socket */
	if (ctrl->data_listen_sd > 0) {
		shutdown(ctrl->data_listen_sd, SHUT_RDWR);
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		ret++;
	}

	/* PASV client socket */
	if (ctrl->data_sd > 0) {
		shutdown(ctrl->data_sd, SHUT_RDWR);
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
		ret++;
	}

	/* PORT */
	if (ctrl->data_address[0]) {
		ctrl->data_address[0] = 0;
		ctrl->data_port = 0;
	}

	return ret;
}

static int check_user_pass(ctrl_t *ctrl)
{
	if (!ctrl->name[0])
		return -1;

	if (!strcmp("anonymous", ctrl->name))
		return 1;

	return 0;
}

static int do_abort(ctrl_t *ctrl)
{
	if (ctrl->d || ctrl->d_num) {
		uev_io_stop(&ctrl->data_watcher);
		if (ctrl->d_num > 0)
			free(ctrl->d);
		ctrl->d_num = 0;
		ctrl->d = NULL;
		ctrl->i = 0;

		if (ctrl->file)
			free(ctrl->file);
		ctrl->file = NULL;
	}

	if (ctrl->file) {
		uev_io_stop(&ctrl->data_watcher);
		free(ctrl->file);
		ctrl->file = NULL;
	}

	if (ctrl->fp) {
		fclose(ctrl->fp);
		ctrl->fp = NULL;
	}

	ctrl->pending = 0;
	ctrl->offset = 0;

	return close_data_connection(ctrl);
}

static void handle_ABOR(ctrl_t *ctrl, char *arg)
{
	DBG("Aborting any current transfer ...");
	if (do_abort(ctrl))
		send_msg(ctrl->sd, "426 Connection closed; transfer aborted.\r\n");

	send_msg(ctrl->sd, "226 Closing data connection.\r\n");
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

static void handle_SYST(ctrl_t *ctrl, char *arg)
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

static void handle_PWD(ctrl_t *ctrl, char *arg)
{
	char buf[sizeof(ctrl->cwd) + 10];

	snprintf(buf, sizeof(buf), "257 \"%s\"\r\n", ctrl->cwd);
	send_msg(ctrl->sd, buf);
}

static void handle_CWD(ctrl_t *ctrl, char *path)
{
	struct stat st;
	char *dir;

	if (!path)
		goto done;

	/*
	 * Some FTP clients, most notably Chrome, use CWD to check if an
	 * entry is a file or directory.
	 */
	dir = compose_abspath(ctrl, path);
	if (!dir || stat(dir, &st) || !S_ISDIR(st.st_mode)) {
		DBG("chrooted:%d, ctrl->cwd: %s, home:%s, dir:%s, len:%zd, dirlen:%zd",
		    chrooted, ctrl->cwd, home, dir, strlen(home), strlen(dir));
		send_msg(ctrl->sd, "550 No such directory.\r\n");
		return;
	}

	if (!chrooted) {
		size_t len = strlen(home);

		DBG("non-chrooted CWD, home:%s, dir:%s, len:%zd, dirlen:%zd",
		    home, dir, len, strlen(dir));
		dir += len;
	}

	snprintf(ctrl->cwd, sizeof(ctrl->cwd), "%s", dir);
	if (ctrl->cwd[0] == 0)
		snprintf(ctrl->cwd, sizeof(ctrl->cwd), "/");

done:
	DBG("New CWD: '%s'", ctrl->cwd);
	send_msg(ctrl->sd, "250 OK\r\n");
}

static void handle_CDUP(ctrl_t *ctrl, char *path)
{
	handle_CWD(ctrl, "..");
}

static void handle_PORT(ctrl_t *ctrl, char *str)
{
	int a, b, c, d, e, f;
	char addr[INET_ADDRSTRLEN];
	struct sockaddr_in sin;

	if (ctrl->data_sd > 0) {
		uev_io_stop(&ctrl->data_watcher);
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

static void handle_EPRT(ctrl_t *ctrl, char *str)
{
	send_msg(ctrl->sd, "502 Command not implemented.\r\n");
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

static char *mlsd_time(time_t mtime)
{
	struct tm *t = localtime(&mtime);
	static char str[20];

	strftime(str, sizeof(str), "%Y%m%d%H%M%S", t);

	return str;
}

static const char *mlsd_type(char *name, int mode)
{
	if (!strcmp(name, "."))
		return "cdir";
	if (!strcmp(name, ".."))
		return "pdir";

	return S_ISDIR(mode) ? "dir" : "file";
}

void mlsd_fact(char fact, char *buf, size_t len, char *name, char *perms, struct stat *st)
{
	char size[20];

	switch (fact) {
	case 'm':
		strlcat(buf, "modify=", len);
		strlcat(buf, mlsd_time(st->st_mtime), len);
		break;

	case 'p':
		strlcat(buf, "perm=", len);
		strlcat(buf, perms, len);
		break;

	case 't':
		strlcat(buf, "type=", len);
		strlcat(buf, mlsd_type(name, st->st_mode), len);
		break;


	case 's':
		if (S_ISDIR(st->st_mode))
			return;
		snprintf(size, sizeof(size), "size=%" PRIu64, st->st_size);
		strlcat(buf, size, len);
		break;

	default:
		return;
	}

	strlcat(buf, ";", len);
}

static void mlsd_printf(ctrl_t *ctrl, char *buf, size_t len, char *path, char *name, struct stat *st)
{
	char perms[10] = "";
	int ro = !access(path, R_OK);
	int rw = !access(path, W_OK);

	if (S_ISDIR(st->st_mode)) {
		/* XXX: Verify 'e' by checking that we can CD to the 'name' */
		if (ro)
			strlcat(perms, "le", sizeof(perms));
		if (rw)
			strlcat(perms, "pc", sizeof(perms)); /* 'd' RMD, 'm' MKD */
	} else {
		if (ro)
			strlcat(perms, "r", sizeof(perms));
		if (rw)
			strlcat(perms, "w", sizeof(perms)); /* 'f' RNFR, 'd' DELE */
	}

	memset(buf, 0, len);
	if (ctrl->d_num == -1 && (ctrl->list_mode & 0x0F) == 2)
		strlcat(buf, " ", len);

	for (int i = 0; ctrl->facts[i]; i++)
		mlsd_fact(ctrl->facts[i], buf, len, name, perms, st);

	strlcat(buf, " ", len);
	strlcat(buf, name, len);
	strlcat(buf, "\r\n", len);
}

static int list_printf(ctrl_t *ctrl, char *buf, size_t len, char *path, char *name)
{
	int dirs;
	int mode = ctrl->list_mode;
	struct stat st;

	if (stat(path, &st))
		return -1;

	dirs = mode & 0xF0;
	mode = mode & 0x0F;

	if (dirs && !S_ISDIR(st.st_mode))
		return 1;
	if (!dirs && S_ISDIR(st.st_mode))
		return 1;

	switch (mode) {
	case 3:			/* MLSD */
		/* fallthrough */
	case 2:			/* MLST */
		mlsd_printf(ctrl, buf, len, path, name, &st);
		break;

	case 1:			/* NLST */
		snprintf(buf, len, "%s\r\n", name);
		break;

	case 0:			/* LIST */
		snprintf(buf, len, "%s 1 %5d %5d %12" PRIu64 " %s %s\r\n",
			 mode_to_str(st.st_mode),
			 0, 0, (uint64_t)st.st_size,
			 time_to_str(st.st_mtime), name);
		break;
	}

	return 0;
}

static void do_MLST(ctrl_t *ctrl)
{
	size_t len = 0;
	char buf[512] = { 0 };
	int sd = ctrl->sd;

	if (ctrl->data_sd != -1)
		sd = ctrl->data_sd;

	snprintf(buf, sizeof(buf), "250- Listing %s\r\n", ctrl->file);
	len = strlen(buf);

	if (list_printf(ctrl, &buf[len], sizeof(buf) -  len, ctrl->file, basename(ctrl->file))) {
		do_abort(ctrl);
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	strlcat(buf, "250 End.\r\n", sizeof(buf));
	send_msg(sd, buf);
}

static void do_MLSD(ctrl_t *ctrl)
{
	char buf[512] = { 0 };

	if (list_printf(ctrl, buf, sizeof(buf), ctrl->file, basename(ctrl->file))) {
		do_abort(ctrl);
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	send_msg(ctrl->data_sd, buf);
	send_msg(ctrl->sd, "226 Transfer complete.\r\n");
}

static void do_LIST(uev_t *w, void *arg, int events)
{
	ctrl_t *ctrl = (ctrl_t *)arg;
	struct timeval tv;
	ssize_t bytes;
	char buf[BUFFER_SIZE] = { 0 };

	if (UEV_ERROR == events || UEV_HUP == events) {
		uev_io_start(w);
		return;
	}

	/* Reset inactivity timer. */
	uev_timer_set(&ctrl->timeout_watcher, INACTIVITY_TIMER, 0);

	if (ctrl->d_num == -1) {
		if (ctrl->list_mode == 3)
			do_MLSD(ctrl);
		else
			do_MLST(ctrl);
		do_abort(ctrl);
		return;
	}

	gettimeofday(&tv, NULL);
	if (tv.tv_sec - ctrl->tv.tv_sec > 3) {
		DBG("Sending LIST entry %d of %d to %s ...", ctrl->i, ctrl->d_num, ctrl->clientaddr);
		ctrl->tv.tv_sec = tv.tv_sec;
	}

	ctrl->list_mode |= (ctrl->pending ? 0 : 0x80);
	while (ctrl->i < ctrl->d_num) {
		struct dirent *entry;
		char *name, *path;
		char cwd[PATH_MAX];

		entry = ctrl->d[ctrl->i++];
		name  = entry->d_name;

		DBG("Found directory entry %s", name);
		if ((!strcmp(name, ".") || !strcmp(name, "..")) && ctrl->list_mode < 2)
			continue;

		snprintf(cwd, sizeof(cwd), "%s%s%s", ctrl->file,
			 ctrl->file[strlen(ctrl->file) - 1] == '/' ? "" : "/", name);
		path = compose_path(ctrl, cwd);
		if (!path) {
		fail:
			LOGIT(LOG_INFO, errno, "Failed reading status for %s", path ? path : name);
			continue;
		}

		switch (list_printf(ctrl, buf, sizeof(buf), path, name)) {
		case -1:
			goto fail;
		case 1:
			continue;
		default:
			break;
		}

		DBG("LIST %s", buf);
		free(entry);

		bytes = send(ctrl->data_sd, buf, strlen(buf), 0);
		if (-1 == bytes) {
			if (ECONNRESET == errno)
				DBG("Connection reset by client.");
			else
				ERR(errno, "Failed sending file %s to client", ctrl->file);

			while (ctrl->i < ctrl->d_num) {
				struct dirent *entry = ctrl->d[ctrl->i++];
				free(entry);
			}
			do_abort(ctrl);
			send_msg(ctrl->sd, "426 TCP connection was established but then broken!\r\n");
		}

		return;
	}
	ctrl->list_mode &= 0x0F;

	/* Rewind and list files */
	if (ctrl->pending == 0) {
		ctrl->pending++;
		ctrl->i = 0;
		return;
	}

	do_abort(ctrl);
	send_msg(ctrl->sd, "226 Transfer complete.\r\n");
}

static void list(ctrl_t *ctrl, char *arg, int mode)
{
	char *path;

	if (string_valid(arg)) {
		char *ptr, *quot;

		/* Check if client sends ls arguments ... */
		ptr = arg;
		while (*ptr) {
			if (isspace(*ptr))
				ptr++;

			if (*ptr == '-') {
				while (*ptr && !isspace(*ptr))
					ptr++;
			}

			break;
		}

		/* Strip any "" from "<arg>" */
		while ((quot = strchr(ptr, '"'))) {
			char *ptr2;

			ptr2 = strchr(&quot[1], '"');
			if (ptr2) {
				memmove(ptr2, &ptr2[1], strlen(ptr2));
				memmove(quot, &quot[1], strlen(quot));
			}
		}
		arg = ptr;
	}

	if (mode >= 2)
		path = compose_abspath(ctrl, arg);
	else
		path = compose_path(ctrl, arg);
	if (!path) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		return;
	}

	ctrl->list_mode = mode;
	ctrl->file = strdup(arg ? arg : "");
	ctrl->i = 0;
	ctrl->d_num = scandir(path, &ctrl->d, NULL, alphasort);
	if (ctrl->d_num == -1) {
		send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		DBG("Failed reading directory '%s': %s", path, strerror(errno));
		return;
	}

	DBG("Reading directory %s ... %d number of entries", path, ctrl->d_num);
	if (ctrl->data_sd > -1) {
		send_msg(ctrl->sd, "125 Data connection already open; transfer starting.\r\n");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_LIST, ctrl, ctrl->data_sd, UEV_WRITE);
		return;
	}

	do_PORT(ctrl, 1);
}

static void handle_LIST(ctrl_t *ctrl, char *arg)
{
	list(ctrl, arg, 0);
}

static void handle_NLST(ctrl_t *ctrl, char *arg)
{
	list(ctrl, arg, 1);
}

static void handle_MLST(ctrl_t *ctrl, char *arg)
{
	list(ctrl, arg, 2);
}

static void handle_MLSD(ctrl_t *ctrl, char *arg)
{
	list(ctrl, arg, 3);
}

static void do_pasv_connection(uev_t *w, void *arg, int events)
{
	ctrl_t *ctrl = (ctrl_t *)arg;
	int rc = 0;

	if (UEV_ERROR == events || UEV_HUP == events) {
		DBG("error on data_listen_sd ...");
		uev_io_start(w);
		return;
	}
	DBG("Event on data_listen_sd ...");
	uev_io_stop(&ctrl->data_watcher);
	if (open_data_connection(ctrl))
		return;

	switch (ctrl->pending) {
	case 3:
		/* fall-through */
	case 2:
		if (ctrl->offset)
			rc = fseek(ctrl->fp, ctrl->offset, SEEK_SET);
		if (rc) {
			do_abort(ctrl);
			send_msg(ctrl->sd, "551 Failed seeking to that position in file.\r\n");
			return;
		}
		/* fall-through */
	case 1:
		break;

	default:
		DBG("No pending command, waiting ...");
		return;
	}

	switch (ctrl->pending) {
	case 3:			/* STOR */
		DBG("Pending STOR, starting ...");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_STOR, ctrl, ctrl->data_sd, UEV_READ);
		break;

	case 2:			/* RETR */
		DBG("Pending RETR, starting ...");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_RETR, ctrl, ctrl->data_sd, UEV_WRITE);
		break;

	case 1:			/* LIST */
		DBG("Pending LIST, starting ...");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_LIST, ctrl, ctrl->data_sd, UEV_WRITE);
		break;
	}

	if (ctrl->pending == 1 && ctrl->list_mode == 2)
		send_msg(ctrl->sd, "150 Opening ASCII mode data connection for MLSD.\r\n");
	else
		send_msg(ctrl->sd, "150 Data connection accepted; transfer starting.\r\n");
	ctrl->pending = 0;
}

static int do_PASV(ctrl_t *ctrl, char *arg, struct sockaddr *data, socklen_t *len)
{
	struct sockaddr_in server;

	if (ctrl->data_sd > 0) {
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
	}

	if (ctrl->data_listen_sd > 0)
		close(ctrl->data_listen_sd);

	ctrl->data_listen_sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (ctrl->data_listen_sd < 0) {
		ERR(errno, "Failed opening data server socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		return 1;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = inet_addr(ctrl->serveraddr);
	server.sin_port        = htons(0);
	if (bind(ctrl->data_listen_sd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		ERR(errno, "Failed binding to client socket");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return 1;
	}

	INFO("Data server port estabished.  Waiting for client to connect ...");
	if (listen(ctrl->data_listen_sd, 1) < 0) {
		ERR(errno, "Client data connection failure");
		send_msg(ctrl->sd, "426 Internal server error.\r\n");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return 1;
	}

	memset(data, 0, sizeof(*data));
	if (-1 == getsockname(ctrl->data_listen_sd, data, len)) {
		ERR(errno, "Cannot determine our address, need it if client should connect to us");
		close(ctrl->data_listen_sd);
		ctrl->data_listen_sd = -1;
		return 1;
	}

	uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_pasv_connection, ctrl, ctrl->data_listen_sd, UEV_READ);

	return 0;
}

static void handle_PASV(ctrl_t *ctrl, char *arg)
{
	struct sockaddr_in data;
	socklen_t len = sizeof(data);
	char *msg, *p, buf[200];
	int port;

	if (do_PASV(ctrl, arg, (struct sockaddr *)&data, &len))
		return;

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

static void handle_EPSV(ctrl_t *ctrl, char *arg)
{
	struct sockaddr_in data;
	socklen_t len = sizeof(data);
	char buf[200];

	if (string_valid(arg) && string_case_compare(arg, "ALL")) {
		send_msg(ctrl->sd, "200 Command OK\r\n");
		return;
	}

	if (do_PASV(ctrl, arg, (struct sockaddr *)&data, &len))
		return;

	snprintf(buf, sizeof(buf), "229 Entering Extended Passive Mode (|||%d|)\r\n", ntohs(data.sin_port));
	send_msg(ctrl->sd, buf);
}

static void do_RETR(uev_t *w, void *arg, int events)
{
	ctrl_t *ctrl = (ctrl_t *)arg;
	struct timeval tv;
	ssize_t bytes;
	size_t num;
	char buf[BUFFER_SIZE];

	if (UEV_ERROR == events || UEV_HUP == events) {
		DBG("error on data_sd ...");
		uev_io_start(w);
		return;
	}

	if (!ctrl->fp) {
		DBG("no fp for RETR, bailing.");
		return;
	}

	num = fread(buf, sizeof(char), sizeof(buf), ctrl->fp);
	if (!num) {
		if (feof(ctrl->fp))
			INFO("User %s from %s downloaded %s", ctrl->name, ctrl->clientaddr, ctrl->file);
		else if (ferror(ctrl->fp))
			ERR(0, "Error while reading %s", ctrl->file);
		do_abort(ctrl);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
		return;
	}

	/* Reset inactivity timer. */
	uev_timer_set(&ctrl->timeout_watcher, INACTIVITY_TIMER, 0);

	gettimeofday(&tv, NULL);
	if (tv.tv_sec - ctrl->tv.tv_sec > 3) {
		DBG("Sending %zd bytes of %s to %s ...", num, ctrl->file, ctrl->clientaddr);
		ctrl->tv.tv_sec = tv.tv_sec;
	}

	bytes = send(ctrl->data_sd, buf, num, 0);
	if (-1 == bytes) {
		if (ECONNRESET == errno)
			DBG("Connection reset by client.");
		else
			ERR(errno, "Failed sending file %s to client", ctrl->file);

		do_abort(ctrl);
		send_msg(ctrl->sd, "426 TCP connection was established but then broken!\r\n");
	}
}

/*
 * Check if previous command was PORT, then connect to client and
 * transfer file/listing similar to what's done for PASV conns.
 */
static void do_PORT(ctrl_t *ctrl, int pending)
{
	if (!ctrl->data_address[0]) {
		/* Check if previous command was PASV */
		if (ctrl->data_sd == -1 && ctrl->data_listen_sd == -1) {
			if (pending == 1 && ctrl->d_num == -1)
				do_MLST(ctrl);
			return;
		}

		ctrl->pending = pending;
		return;
	}

	if (open_data_connection(ctrl)) {
		do_abort(ctrl);
		send_msg(ctrl->sd, "425 TCP connection cannot be established.\r\n");
		return;
	}

	if (pending != 1 || ctrl->list_mode != 2)
		send_msg(ctrl->sd, "150 Data connection opened; transfer starting.\r\n");

	switch (pending) {
	case 3:
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_STOR, ctrl, ctrl->data_sd, UEV_READ);
		break;

	case 2:
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_RETR, ctrl, ctrl->data_sd, UEV_WRITE);
		break;

	case 1:
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_LIST, ctrl, ctrl->data_sd, UEV_WRITE);
		break;
	}

	ctrl->pending = 0;
}

static void handle_RETR(ctrl_t *ctrl, char *file)
{
	FILE *fp;
	char *path;
	struct stat st;

	path = compose_abspath(ctrl, file);
	if (!path || stat(path, &st) || !S_ISREG(st.st_mode)) {
		send_msg(ctrl->sd, "550 Not a regular file.\r\n");
		return;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		if (errno != ENOENT)
			ERR(errno, "Failed RETR %s for %s", path, ctrl->clientaddr);
		send_msg(ctrl->sd, "451 Trouble to RETR file.\r\n");
		return;
	}

	ctrl->fp = fp;
	ctrl->file = strdup(file);

	if (ctrl->data_sd > -1) {
		if (ctrl->offset) {
			DBG("Previous REST %ld of file size %ld", ctrl->offset, st.st_size);
			if (fseek(fp, ctrl->offset, SEEK_SET)) {
				do_abort(ctrl);
				send_msg(ctrl->sd, "551 Failed seeking to that position in file.\r\n");
				return;
			}
		}

		send_msg(ctrl->sd, "125 Data connection already open; transfer starting.\r\n");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_RETR, ctrl, ctrl->data_sd, UEV_WRITE);
		return;
	}

	do_PORT(ctrl, 2);
}

static void handle_MDTM(ctrl_t *ctrl, char *file)
{
	struct stat st;
	struct tm *tm;
	char *path, *ptr;
	char *mtime = NULL;
	char buf[80];

	/* Request to set mtime, ncftp does this */
	ptr = strchr(file, ' ');
	if (ptr) {
		*ptr++ = 0;
		mtime = file;
		file  = ptr;
	}

	path = compose_abspath(ctrl, file);
	if (!path || stat(path, &st) || !S_ISREG(st.st_mode)) {
		send_msg(ctrl->sd, "550 Not a regular file.\r\n");
		return;
	}

	if (mtime) {
		struct timespec times[2] = {
			{ 0, UTIME_OMIT },
			{ 0, 0 }
		};
		struct tm tm;
		int rc;

		if (!strptime(mtime, "%Y%m%d%H%M%S", &tm)) {
		fail:
			send_msg(ctrl->sd, "550 Invalid time format\r\n");
			return;
		}

		times[1].tv_sec = mktime(&tm);
		rc = utimensat(0, path, times, 0);
		if (rc) {
			ERR(errno, "Failed setting MTIME %s of %s", mtime, file);
			goto fail;
		}
		(void)stat(path, &st);
	}

	tm = gmtime(&st.st_mtime);
	strftime(buf, sizeof(buf), "213 %Y%m%d%H%M%S\r\n", tm);

	send_msg(ctrl->sd, buf);
}

static void do_STOR(uev_t *w, void *arg, int events)
{
	ctrl_t *ctrl = (ctrl_t *)arg;
	struct timeval tv;
	ssize_t bytes;
	size_t num;
	char buf[BUFFER_SIZE];

	if (UEV_ERROR == events || UEV_HUP == events) {
		DBG("error on data_sd ...");
		uev_io_start(w);
		return;
	}

	if (!ctrl->fp) {
		DBG("no fp for STOR, bailing.");
		return;
	}

	/* Reset inactivity timer. */
	uev_timer_set(&ctrl->timeout_watcher, INACTIVITY_TIMER, 0);

	bytes = recv(ctrl->data_sd, buf, sizeof(buf), 0);
	if (bytes < 0) {
		if (ECONNRESET == errno)
			DBG("Connection reset by client.");
		else
			ERR(errno, "Failed receiving file %s from client", ctrl->file);
		do_abort(ctrl);
		send_msg(ctrl->sd, "426 TCP connection was established but then broken!\r\n");
		return;
	}
	if (bytes == 0) {
		INFO("User %s at %s uploaded file %s", ctrl->name, ctrl->clientaddr, ctrl->file);
		do_abort(ctrl);
		send_msg(ctrl->sd, "226 Transfer complete.\r\n");
		return;
	}

	gettimeofday(&tv, NULL);
	if (tv.tv_sec - ctrl->tv.tv_sec > 3) {
		DBG("Receiving %zd bytes of %s from %s ...", bytes, ctrl->file, ctrl->clientaddr);
		ctrl->tv.tv_sec = tv.tv_sec;
	}

	num = fwrite(buf, 1, bytes, ctrl->fp);
	if ((size_t)bytes != num)
		ERR(errno, "552 Disk full.");
}

static void handle_STOR(ctrl_t *ctrl, char *file)
{
	FILE *fp = NULL;
	char *path;
	int rc = 0;

	path = compose_abspath(ctrl, file);
	if (!path) {
		INFO("Invalid path for %s: %m", file);
		goto fail;
	}

	DBG("Trying to write to %s ...", path);
	fp = fopen(path, "wb");
	if (!fp) {
		/* If EACCESS client is trying to do something disallowed */
		ERR(errno, "Failed writing %s", path);
	fail:
		send_msg(ctrl->sd, "451 Trouble storing file.\r\n");
		do_abort(ctrl);
		return;
	}

	ctrl->fp = fp;
	ctrl->file = strdup(file);

	if (ctrl->data_sd > -1) {
		if (ctrl->offset)
			rc = fseek(fp, ctrl->offset, SEEK_SET);
		if (rc) {
			do_abort(ctrl);
			send_msg(ctrl->sd, "551 Failed seeking to that position in file.\r\n");
			return;
		}

		send_msg(ctrl->sd, "125 Data connection already open; transfer starting.\r\n");
		uev_io_init(ctrl->ctx, &ctrl->data_watcher, do_STOR, ctrl, ctrl->data_sd, UEV_READ);
		return;
	}

	do_PORT(ctrl, 3);
}

static void handle_DELE(ctrl_t *ctrl, char *file)
{
	char *path;

	path = compose_abspath(ctrl, file);
	if (!path) {
		ERR(errno, "Cannot find %s", file);
		goto fail;
	}

	if (remove(path)) {
		if (ENOENT == errno)
		fail:	send_msg(ctrl->sd, "550 No such file or directory.\r\n");
		else if (EPERM == errno)
			send_msg(ctrl->sd, "550 Not allowed to remove file or directory.\r\n");
		else
			send_msg(ctrl->sd, "550 Unknown error.\r\n");
		return;
	}

	send_msg(ctrl->sd, "200 Command OK\r\n");
}

static void handle_MKD(ctrl_t *ctrl, char *arg)
{
	char *path;

	path = compose_abspath(ctrl, arg);
	if (!path) {
		INFO("Invalid path for %s: %m", arg);
		goto fail;
	}

	if (mkdir(path, 0755)) {
		if (EPERM == errno)
		fail:	send_msg(ctrl->sd, "550 Not allowed to create directory.\r\n");
		else
			send_msg(ctrl->sd, "550 Unknown error.\r\n");
		return;
	}

	send_msg(ctrl->sd, "200 Command OK\r\n");
}

static void handle_RMD(ctrl_t *ctrl, char *arg)
{
	handle_DELE(ctrl, arg);
}

static void handle_REST(ctrl_t *ctrl, char *arg)
{
	const char *errstr;
	char buf[80];

	if (!string_valid(arg)) {
		send_msg(ctrl->sd, "550 Invalid argument.\r\n");
		return;
	}

	ctrl->offset = strtonum(arg, 0, INT64_MAX, &errstr);
	snprintf(buf, sizeof(buf), "350 Restarting at %ld.  Send STOR or RETR to continue transfer.\r\n", ctrl->offset);
	send_msg(ctrl->sd, buf);
}

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
	char *path;
	char buf[80];
	size_t extralen = 0;
	struct stat st;

	path = compose_abspath(ctrl, file);
	if (!path || stat(path, &st) || S_ISDIR(st.st_mode)) {
		send_msg(ctrl->sd, "550 No such file, or argument is a directory.\r\n");
		return;
	}

	DBG("SIZE %s", path);

	if (ctrl->type == TYPE_A)
		extralen = num_nl(path);

	snprintf(buf, sizeof(buf), "213 %"  PRIu64 "\r\n", (uint64_t)(st.st_size + extralen));
	send_msg(ctrl->sd, buf);
}

/* No operation - used as session keepalive by clients. */
static void handle_NOOP(ctrl_t *ctrl, char *arg)
{
	send_msg(ctrl->sd, "200 NOOP OK.\r\n");
}

#if 0
static void handle_RNFR(ctrl_t *ctrl, char *arg)
{
}

static void handle_RNTO(ctrl_t *ctrl, char *arg)
{
}
#endif

static void handle_QUIT(ctrl_t *ctrl, char *arg)
{
	send_msg(ctrl->sd, "221 Goodbye.\r\n");
	uev_exit(ctrl->ctx);
}

static void handle_CLNT(ctrl_t *ctrl, char *arg)
{
	send_msg(ctrl->sd, "200 CLNT\r\n");
}

static void handle_OPTS(ctrl_t *ctrl, char *arg)
{
	/* OPTS MLST type;size;modify;perm; */
	if (strstr(arg, "MLST")) {
		size_t i = 0;
		char *ptr;
		char buf[42] = "200 MLST OPTS ";
		char facts[10] = { 0 };

		ptr = strtok(arg + 4, " \t;");
		while (ptr && i < sizeof(facts) - 1) {
			if (!strcmp(ptr, "modify") ||
			    !strcmp(ptr, "perm")   ||
			    !strcmp(ptr, "size")   ||
			    !strcmp(ptr, "type")) {
				facts[i++] = ptr[0];
				strlcat(buf, ptr, sizeof(buf));
				strlcat(buf, ";", sizeof(buf));
			}

			ptr = strtok(NULL, ";");
		}
		strlcat(buf, "\r\n", sizeof(buf));

		DBG("New MLSD facts: %s", facts);
		strlcpy(ctrl->facts, facts, sizeof(ctrl->facts));
		send_msg(ctrl->sd, buf);
	} else
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

static void handle_FEAT(ctrl_t *ctrl, char *arg)
{
	snprintf(ctrl->buf, ctrl->bufsz, "211-Features:\r\n"
		 " EPSV\r\n"
		 " PASV\r\n"
		 " SIZE\r\n"
		 " UTF8\r\n"
		 " REST STREAM\r\n"
		 " MLST modify*;perm*;size*;type*;\r\n"
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
	COMMAND(ABOR),
	COMMAND(DELE),
	COMMAND(USER),
	COMMAND(PASS),
	COMMAND(SYST),
	COMMAND(TYPE),
	COMMAND(PORT),
	COMMAND(EPRT),
	COMMAND(RETR),
	COMMAND(MKD),
	COMMAND(RMD),
	COMMAND(REST),
	COMMAND(MDTM),
	COMMAND(PASV),
	COMMAND(EPSV),
	COMMAND(QUIT),
	COMMAND(LIST),
	COMMAND(NLST),
	COMMAND(MLST),
	COMMAND(MLSD),
	COMMAND(CLNT),
	COMMAND(OPTS),
	COMMAND(PWD),
	COMMAND(STOR),
	COMMAND(CWD),
	COMMAND(CDUP),
	COMMAND(SIZE),
	COMMAND(NOOP),
	COMMAND(HELP),
	COMMAND(FEAT),
	{ NULL, NULL }
};

static void child_exit(uev_t *w, void *arg, int events)
{
	DBG("Child exiting ...");
	uev_exit(w->ctx);
}

static void read_client_command(uev_t *w, void *arg, int events)
{
	char *command, *argument;
	ctrl_t *ctrl = (ctrl_t *)arg;
	ftp_cmd_t *cmd;

	if (UEV_ERROR == events || UEV_HUP == events) {
		uev_io_start(w);
		return;
	}

	/* Reset inactivity timer. */
	uev_timer_set(&ctrl->timeout_watcher, INACTIVITY_TIMER, 0);

	if (recv_msg(w->fd, ctrl->buf, ctrl->bufsz, &command, &argument)) {
		DBG("Short read, exiting.");
		uev_exit(ctrl->ctx);
		return;
	}

	if (!string_valid(command))
		return;

	if (string_match(command, "FF F4")) {
		DBG("Ignoring IAC command, client should send ABOR as well.");
		return;
	}

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
		ERR(errno, "Cannot determine our address");
		goto fail;
	}
	convert_address(&ctrl->server_sa, ctrl->serveraddr, sizeof(ctrl->serveraddr));

	len = sizeof(ctrl->client_sa);
	if (-1 == getpeername(sd, (struct sockaddr *)&ctrl->client_sa, &len)) {
		ERR(errno, "Cannot determine client address");
		goto fail;
	}
	convert_address(&ctrl->client_sa, ctrl->clientaddr, sizeof(ctrl->clientaddr));

	ctrl->type = TYPE_A;
	ctrl->data_listen_sd = -1;
	ctrl->data_sd = -1;
	ctrl->name[0] = 0;
	ctrl->pass[0] = 0;
	ctrl->data_address[0] = 0;
	strlcpy(ctrl->facts, "mpst", sizeof(ctrl->facts));

	INFO("Client connection from %s", ctrl->clientaddr);
	ftp_command(ctrl);

	DBG("Client exiting, bye");
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
