/* Common methods shared between FTP and TFTP engines
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

int chrooted = 0;

/* Protect against common directory traversal attacks, for details see
 * https://en.wikipedia.org/wiki/Directory_traversal_attack
 *
 * Example:            /srv/ftp/ ../../etc => /etc
 *                    .~~~~~~~~ .~~~~~~~~~
 *                   /         /
 * Server dir ------'         /
 * User input ---------------'
 *
 * Forced dir ------> /srv/ftp/etc
 */
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

	/* Protect against directory traversal attacks */
	squash_dots(dir);

	if (!chrooted) {
		size_t len = strlen(home);

		DBG("Server path from CWD: %s", dir);
		if (len > 0 && home[len - 1] == '/')
			len--;
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
		WARN(errno, "Failed setting SO_REUSEADDR on %s socket", type == SOCK_DGRAM ? "TFTP" : "FTP");

	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port        = htons(port);
	if (bind(sd, (struct sockaddr *)&server, len) < 0) {
		if (EACCES != errno) {
			WARN(errno, "Failed binding to port %d, maybe another %s server is already running", port, desc);
		}
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
			  &((struct sockaddr_in6 *)ss)->sin6_addr, buf, len);
		break;
	}
}

/* Inactivity timer, bye bye */
static void inactivity_cb(uev_t *UNUSED(w), void *arg, int UNUSED(events))
{
	uev_ctx_t *ctx = (uev_ctx_t *)arg;

	INFO("Inactivity timer, exiting ...");
	uev_exit(ctx);
}

ctrl_t *new_session(uev_ctx_t *ctx, int sd, int *rc)
{
	ctrl_t *ctrl = NULL;
	static int privs_dropped = 0;

	if (!inetd) {
		pid_t pid = fork();

		if (pid) {
			DBG("Created new client session as PID %d", pid);
			*rc = pid;
			return NULL;
		}

		/* Create new uEv context for the child. */
		ctx = calloc(1, sizeof(uev_ctx_t));
		if (!ctx) {
			ERR(errno, "Failed allocating session event context");
			exit(1);
		}

		uev_init(ctx);
	}

	ctrl = calloc(1, sizeof(ctrl_t));
	if (!ctrl) {
		ERR(errno, "Failed allocating session context");
		goto fail;
	}

	ctrl->sd = sd;
	ctrl->ctx = ctx;
	strlcpy(ctrl->cwd, "/", sizeof(ctrl->cwd));

	/* Chroot to FTP root */
	if (!chrooted && geteuid() == 0) {
		if (chroot(home) || chdir("/")) {
			ERR(errno, "Failed chrooting to FTP root, %s, aborting", home);
			goto fail;
		}
		chrooted = 1;
	} else if (!chrooted) {
		if (chdir(home)) {
			WARN(errno, "Failed changing to FTP root, %s, aborting", home);
			goto fail;
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

	/* Session timeout handler */
	uev_timer_init(ctrl->ctx, &ctrl->timeout_watcher, inactivity_cb, ctrl->ctx, INACTIVITY_TIMER, 0);

	return ctrl;
fail:
	if (ctrl)
		free(ctrl);
	if (!inetd)
		free(ctx);
	*rc = -1;

	return NULL;
}

int del_session(ctrl_t *ctrl, int isftp)
{
	uev_ctx_t *ctx;

	DBG("%sFTP Client session ended.", isftp ? "": "T" );

	if (!ctrl)
		return -1;
	ctx = ctrl->ctx;

	if (isftp && ctrl->sd > 0) {
		shutdown(ctrl->sd, SHUT_RDWR);
		close(ctrl->sd);
	}

	if (ctrl->data_listen_sd > 0) {
		shutdown(ctrl->data_listen_sd, SHUT_RDWR);
		close(ctrl->data_listen_sd);
	}

	if (ctrl->data_sd > 0) {
		shutdown(ctrl->data_sd, SHUT_RDWR);
		close(ctrl->data_sd);
	}

	if (ctrl->buf)
		free(ctrl->buf);

	if (!inetd && ctrl->ctx)
		free(ctrl->ctx);
	free(ctrl);

	if (!inetd)
		return uev_exit(ctx);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
