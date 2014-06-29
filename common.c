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
static uev_t ftp;
static uev_t tftp;

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

char *compose_path(ctx_t *ctrl, char *path)
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

/* Check for any forked off children that exited. */
void collect_sessions(void)
{
	while (1) {
		pid_t pid;

		pid = waitpid(0, NULL, WNOHANG);
		if (!pid)
			continue;

		if (-1 == pid)
			break;
	}
}

static int open_socket(int port, int type, char *desc)
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

	server.sin_family      = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port        = htons(port);
	len                    = sizeof(struct sockaddr);
	if (bind(sd, (struct sockaddr *)&server, len) < 0) {
		WARN(errno, "Failed binding to port %d, maybe another %s server is already running", port, desc);
		close(sd);

		return -1;
	}

	if (port) {
		if (-1 == listen(sd, 20))
			WARN(errno, "Failed starting %s server", desc);
	}

	return sd;
}

/* Inactivity timer, bye bye */
void sigalrm_handler(int UNUSED(signo), siginfo_t *UNUSED(info), void *UNUSED(ctx))
{
	INFO("Inactivity timer, exiting ...");
	exit(0);
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
		int fail1, fail2;

		initgroups (pw->pw_name, pw->pw_gid);
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

	ftp_command(ctrl);
	stop_session(ctrl);

	DBG("Exiting ...");
	exit(0);
}


int ftp_session(int sd)
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

static void ftp_cb(uev_ctx_t *UNUSED(ctx), uev_t *w, void UNUSED(*arg), int UNUSED(events))
{
        int client;

        client = accept(w->fd, NULL, NULL);
        if (client < 0) {
                perror("Failed accepting incoming FTP client connection");
                return;
        }

        ftp_session(client);
        collect_sessions();
}

static void tftp_cb(uev_ctx_t *UNUSED(ctx), uev_t *w, void UNUSED(*arg), int UNUSED(events))
{
        /* XXX: Fork me here ... to prevent blocking new connections during a TFTP session */
        tftp_session(w->fd);
}

int serve_files(void)
{
        int sd;
        uev_ctx_t uc;

        uev_init(&uc);

	sd = open_socket(port, SOCK_STREAM, "FTP");
	if (sd < 0)
		exit(1);

        uev_io_init(&uc, &ftp, ftp_cb, NULL, sd, UEV_READ);
        INFO("Started FTP server on port %d", port);

	if (do_tftp) {
		sd = open_socket(69, SOCK_DGRAM, "TFTP");	/* XXX: Fix hardcoded port n:o */
                if (sd < 0) {
                        WARN(errno, "Failed starting TFTP service");
                } else {
                        uev_io_init(&uc, &tftp, tftp_cb, NULL, sd, UEV_READ);
                        INFO("Started TFTP server on port 69");
                }
        }

	INFO("Serving files from %s ...", home);

	return uev_run(&uc, 0);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
