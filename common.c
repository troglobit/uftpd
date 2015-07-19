/* Common methods shared between FTP and TFTP engines
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

#include "uftpd.h"

int chrooted = 0;

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

char *compose_path(ctrl_t *ctrl, char *path)
{
	static char dir[PATH_MAX];

	strlcpy(dir, ctrl->cwd, sizeof(dir));

	DBG("Compose path from cwd: %s, arg: %s", ctrl->cwd, path ?: "");
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

int open_socket(int port, int type, char *desc)
{
	int sd, err, val = 1;
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in server;

	sd = socket(AF_INET, type | SOCK_NONBLOCK, 0);
	if (sd < 0) {
		WARN(errno, "Failed creating %s server socket", desc);
		return -1;
	}

	err = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));
	if (err != 0)
		WARN(errno, "Failed setting SO_REUSEADDR on TFTP socket");

	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port        = htons(port);
	if (bind(sd, (struct sockaddr *)&server, len) < 0) {
		WARN(errno, "Failed binding to port %d, maybe another %s server is already running", port, desc);
		close(sd);

		return -1;
	}

	if (port && type != SOCK_DGRAM) {
		if (-1 == listen(sd, 20))
			WARN(errno, "Failed starting %s server", desc);
	}

	DBG("Opened socket for port %d!", port);

	return sd;
}

void convert_address(struct sockaddr_storage *ss, char *buf, size_t len)
{
	switch (ss->ss_family) {
	case AF_INET:
		inet_ntop(ss->ss_family,
			  &((struct sockaddr_in *)ss)->sin_addr, buf, len);
		break;

	case AF_INET6:
		inet_ntop(ss->ss_family,
			  &((struct sockaddr_in6 *)&ss)->sin6_addr, buf, len);
		break;
	}
}

/* Inactivity timer, bye bye */
void sigalrm_handler(int UNUSED(signo), siginfo_t *UNUSED(info), void *UNUSED(ctx))
{
	INFO("Inactivity timer, exiting ...");
	exit(0);
}


ctrl_t *new_session(int sd, int *rc)
{
	ctrl_t *ctrl;
	static int privs_dropped = 0;

	if (!inetd) {
		pid_t pid = fork();

		if (pid) {
			DBG("Created new client session as PID %d", pid);
			*rc = pid;
			return NULL;
		}
	}

	ctrl = calloc(1, sizeof(ctrl_t));
	if (!ctrl) {
		ERR(errno, "Failed allocating session context");
		*rc = -1;
		return NULL;
	}

	ctrl->sd = sd;
	strlcpy(ctrl->cwd, "/", sizeof(ctrl->cwd));

	/* Chroot to FTP root */
	if (!chrooted && geteuid() == 0) {
		if (chroot(home) || chdir("/")) {
			ERR(errno, "Failed chrooting to FTP root, %s, aborting", home);
			free(ctrl);
			*rc = -1;
			return NULL;
		}
		chrooted = 1;
	} else if (!chrooted) {
		if (chdir(home)) {
			WARN(errno, "Failed changing to FTP root, %s, aborting", home);
			free(ctrl);
			*rc = -1;
			return NULL;
		}
	}

	/* If ftp user exists and we're running as root we can drop privs */
	if (!privs_dropped && pw && geteuid() == 0) {
		int fail1, fail2;

		initgroups(pw->pw_name, pw->pw_gid);
		if ((fail1 = setegid(pw->pw_gid)))
			WARN(errno, "Failed dropping group privileges to gid %d", pw->pw_gid);
		if ((fail2 = seteuid(pw->pw_uid)))
			WARN(errno, "Failed dropping user privileges to uid %d", pw->pw_uid);

		setenv("HOME", pw->pw_dir, 1);

		if (!fail1 && !fail2)
			INFO("Successfully dropped privilges to %d:%d (uid:gid)", pw->pw_uid, pw->pw_gid);

		/* On failure, we tried at least.  Only warn once. */
		privs_dropped = 1;
	}

	return ctrl;
}

int del_session(ctrl_t *ctrl)
{
	if (!ctrl)
		return -1;

	if (ctrl->sd > 0)
		close(ctrl->sd);

	if (ctrl->data_listen_sd > 0)
		close(ctrl->data_listen_sd);

	if (ctrl->data_sd > 0)
		close(ctrl->data_sd);

	free(ctrl);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
