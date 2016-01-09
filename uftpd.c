/* uftpd -- the no nonsense (T)FTP server
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

/* Global daemon settings */
char *home        = NULL;
int   inetd       = 0;
int   background  = 1;
int   do_syslog   = 1;
int   do_ftp      = FTP_DEFAULT_PORT;
int   do_tftp     = TFTP_DEFAULT_PORT;
pid_t tftp_pid    = 0;
struct passwd *pw = NULL;

/* Event contexts */
static uev_t ftp_watcher;
static uev_t tftp_watcher;
static uev_t sigchld_watcher;
static uev_t sigterm_watcher;
static uev_t sigint_watcher;
static uev_t sighup_watcher;
static uev_t sigquit_watcher;


static int version(void)
{
	printf("Version %s\n", VERSION);
	return 0;
}

static int usage(int code)
{
	int is_inetd = string_match(__progname, "in.");

	if (is_inetd)
		printf("\nUsage: %s [-hv] [-l LEVEL] [PATH]\n\n", __progname);
	else
		printf("\nUsage: %s [-hnsv] [-l LEVEL] [-o ftp=PORT,tftp=PORT] [PATH]\n\n", __progname);

	printf("  -h         Show this help text\n"
	       "  -l LEVEL   Set log level: none, err, info, notice (default), debug\n");
	if (!is_inetd)
		printf("  -n         Run in foreground, do not detach from controlling terminal\n"
		       "  -o OPT     Options:\n"
		       "                      ftp=PORT\n"
		       "                      tftp=PORT\n"
		       "  -s         Use syslog, even if running in foreground, default w/o -n\n");

	printf("  -v         Show program version\n\n");
	printf("The optional FTP/TFTP 'PATH' defaults to the FTP user's $HOME\n"
	       "Bug report address: %-40s\n\n", BUGADDR);

	return code;
}

/*
 * SIGCHLD: one of our children has died
 */
static void sigchld_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	while (1) {
		pid_t pid;

		pid = waitpid(0, NULL, WNOHANG);
		if (!pid)
			continue;

		if (-1 == pid)
			break;

		/* TFTP client disconnected, we can now serve TFTP again! */
		if (pid == tftp_pid) {
			DBG("Previous TFTP session ended, restarting TFTP watcher ...");
			tftp_pid = 0;
			uev_io_start(&tftp_watcher);
		}
	}
}

/*
 * SIGQUIT: request termination
 */
static void sigquit_cb(uev_t *w, void *UNUSED(arg), int UNUSED(events))
{
	INFO("Recieved signal %d, exiting ...", w->signo);

	/* Forward signal to any children in this process group. */
	killpg(0, SIGTERM);
	sched_yield();		/* Give them time to exit gracefully. */

	/* Leave main loop. */
	uev_exit(w->ctx);
}

static void sig_init(uev_ctx_t *ctx)
{
	uev_signal_init(ctx, &sigchld_watcher, sigchld_cb, NULL, SIGCHLD);
	uev_signal_init(ctx, &sigterm_watcher, sigquit_cb, NULL, SIGTERM);
	uev_signal_init(ctx, &sigint_watcher,  sigquit_cb, NULL, SIGINT);
	uev_signal_init(ctx, &sighup_watcher,  sigquit_cb, NULL, SIGHUP);
	uev_signal_init(ctx, &sigquit_watcher, sigquit_cb, NULL, SIGQUIT);
}

static int find_port(char *service, char *proto, int fallback)
{
	int port = fallback;
	struct servent *sv;

	sv = getservbyname(service, proto);
	if (!sv)
		WARN(errno, "Cannot find service %s/%s, defaulting to %d.", service, proto, port);
	else
		port = ntohs(sv->s_port);

	DBG("Found port %d for service %s, proto %s (fallback port %d)", port, service, proto, fallback);

	return port;
}

static void init(uev_ctx_t *ctx)
{
	uev_init(ctx);

	/* Figure out FTP/TFTP ports */
	if (do_ftp == 1)
		do_ftp  = find_port(FTP_SERVICE_NAME, FTP_PROTO_NAME, FTP_DEFAULT_PORT);
	if (do_tftp == 1)
		do_tftp = find_port(TFTP_SERVICE_NAME, TFTP_PROTO_NAME, TFTP_DEFAULT_PORT); 

	/* Figure out FTP home directory */
	if (!home) {
		pw = getpwnam(FTP_DEFAULT_USER);
		if (!pw) {
			home = strdup(FTP_DEFAULT_HOME);
			WARN(errno, "Cannot find user %s, falling back to %s as FTP root.",
			     FTP_DEFAULT_USER, home);
		} else {
			home = strdup(pw->pw_dir);
		}
	}
}

static void ftp_cb(uev_t *w, void *arg, int UNUSED(events))
{
        int client;

        client = accept(w->fd, NULL, NULL);
        if (client < 0) {
                WARN(errno, "Failed accepting FTP client connection");
                return;
        }

        ftp_session(arg, client);
}

static void tftp_cb(uev_t *w, void *arg, int UNUSED(events))
{
	uev_io_stop(w);

        tftp_pid = tftp_session(arg, w->fd);
	if (tftp_pid < 0) {
		tftp_pid = 0;
		uev_io_start(w);
	}
}

static int start_service(uev_ctx_t *ctx, uev_t *w, uev_cb_t *cb, int port, int type, char *desc)
{
	int sd;

	if (!port)
		/* Disabled */
		return 1;

	sd = open_socket(port, type, desc);
	if (sd < 0) {
		WARN(errno, "Failed starting %s service", desc);
		return 1;
	}

	INFO("Starting %s server on port %d ...", desc, port);
	uev_io_init(ctx, w, cb, ctx, sd, UEV_READ);

	return 0;
}

static int serve_files(uev_ctx_t *ctx)
{
	int ftp, tftp;

	DBG("Starting services ...");
	ftp  = start_service(ctx, &ftp_watcher,   ftp_cb, do_ftp, SOCK_STREAM, "FTP");
	tftp = start_service(ctx, &tftp_watcher, tftp_cb, do_tftp, SOCK_DGRAM, "TFTP");

	/* Check if failed to start any service ... */
	if (ftp && tftp)
		return 1;

	/* Setup signal callbacks */
	sig_init(ctx);

	/* We're now up and running, save pid file. */
	pidfile(NULL);

	INFO("Serving files from %s ...", home);

	return uev_run(ctx, 0);
}

int main(int argc, char **argv)
{
	int c;
	enum {
		FTP_OPT = 0,
		TFTP_OPT,
	};
	char *subopts;
	char *const token[] = {
		[FTP_OPT]   = "ftp",
		[TFTP_OPT]  = "tftp",
		NULL
	};
	uev_ctx_t ctx;

	while ((c = getopt(argc, argv, "hl:no:sv")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);

		case 'l':
			loglevel = loglvl(optarg);
			if (-1 == loglevel)
				return usage(1);
			break;

		case 'n':
			background = 0;
			do_syslog--;
			break;

		case 'o':
			subopts = optarg;
			while (*subopts != '\0') {
				char *value;

				switch (getsubopt(&subopts, token, &value)) {
				case FTP_OPT:
					if (!value) {
						fprintf(stderr, "Missing port argument to -o ftp=PORT\n");
						return usage(1);
					}
					do_ftp = atoi(value);
					break;

				case TFTP_OPT:
					if (!value) {
						fprintf(stderr, "Missing port argument to -o tftp=PORT\n");
						return usage(1);
					}
					do_tftp = atoi(value);
					break;

				default:
					fprintf(stderr, "Unrecognized option '%s'\n", value);
					return usage(1);
				}
			}
			break;

		case 's':
			do_syslog++;
			break;

		case 'v':
			return version();

		default:
			return usage(1);
		}
	}

	if (optind < argc)
		home = strdup(argv[optind]);

	/* Inetd mode enforces foreground and syslog */
	if (string_compare(__progname, "in.tftpd")) {
		inetd      = 1;
		do_ftp     = 0;
		do_tftp    = 1;
		background = 0;
		do_syslog  = 1;
	} else if (string_compare(__progname, "in.ftpd")) {
		inetd      = 1;
		do_ftp     = 1;
		do_tftp    = 0;
		background = 0;
		do_syslog  = 1;
	}

	if (do_syslog) {
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_FTP);
		setlogmask(LOG_UPTO(loglevel));
	}

	DBG("Initializing ...");
	init(&ctx);

	if (inetd) {
		pid_t pid;

		INFO("Started from inetd, serving files from %s ...", home);
		if (do_tftp)
			pid = tftp_session(&ctx, STDIN_FILENO);
		else
			pid = ftp_session(&ctx, STDIN_FILENO);

		if (-1 == pid)
			return 1;
		return 0;
	}

	if (background) {
		DBG("Daemonizing ...");
		if (-1 == daemon(0, 0)) {
			ERR(errno, "Failed daemonizing");
			return 1;
		}
	}

	DBG("Serving files as PID %d ...", getpid());
	return serve_files(&ctx);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
