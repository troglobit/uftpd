/* uftpd -- the no nonsense (T)FTP server
 *
 * Copyright (c) 2014-2021  Joachim Wiberg <troglobit@gmail.com>
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
char *prognm      = PACKAGE_NAME;
char *pidfn       = NULL;
char *home        = NULL;
int   inetd       = 0;
int   background  = 1;
int   do_syslog   = 1;
int   do_ftp      = FTP_DEFAULT_PORT;
int   do_tftp     = TFTP_DEFAULT_PORT;
char *pasv_addr   = NULL;
int   do_insecure = 0;
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
	printf("%s\n", PACKAGE_VERSION);
	printf("\nBug report address: %s\n", PACKAGE_BUGREPORT);
#ifdef PACKAGE_URL
	printf("Project homepage: %s\n", PACKAGE_URL);
#endif
	return 0;
}

static int usage(int code)
{
	int is_inetd = string_match(prognm, "in.");

	if (is_inetd)
		printf("\nUsage: %s [-hv] [-l LEVEL] [PATH]\n\n", prognm);
	else
		printf("\nUsage: %s [-hnsv] [-l LEVEL] [-o OPTS] [-p FILE] [PATH]\n\n", prognm);

	printf("  -h         Show this help text\n"
	       "  -l LEVEL   Set log level: none, err, notice (default), info, debug\n");
	if (!is_inetd)
		printf("  -n         Run in foreground, do not detach from controlling terminal\n"
		       "  -o OPT     Options:\n"
		       "                      ftp=PORT\n"
		       "                      tftp=PORT\n"
		       "                      pasv_addr=ADDR\n"
		       "                      writable\n"
		       "  -p FILE    File to store process ID for signaling %s\n"
		       "  -s         Use syslog, even if running in foreground, default w/o -n\n",
		       prognm);

	printf("  -v         Show program version\n\n");
	printf("The optional 'PATH' defaults to the $HOME of the /etc/passwd user 'ftp'\n");

	return code;
}

/*
 * SIGCHLD: one of our children has died
 */
static void sigchld_cb(uev_t *w, void *arg, int events)
{
	while (1) {
		pid_t pid;

		pid = waitpid(0, NULL, WNOHANG);
		if (pid <= 0)
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
static void sigquit_cb(uev_t *w, void *arg, int events)
{
	INFO("Received signal %d, exiting ...", w->signo);

	/* Forward signal to any children in this process group. */
	if (killpg(getpgrp(), SIGTERM))
		WARN(errno, "Failed signalling children");

	/* Give them time to exit gracefully. */
	while (wait(NULL) != -1)
		;

	if (home)
		free(home);

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

static int init(uev_ctx_t *ctx)
{
	/* Figure out FTP/TFTP ports */
	if (do_ftp == 1)
		do_ftp  = find_port(FTP_SERVICE_NAME, FTP_PROTO_NAME, FTP_DEFAULT_PORT);
	if (do_tftp == 1)
		do_tftp = find_port(TFTP_SERVICE_NAME, TFTP_PROTO_NAME, TFTP_DEFAULT_PORT); 

	/* Figure out FTP home directory */
	if (!home) {
		pw = getpwnam(FTP_DEFAULT_USER);
		if (!pw) {
			WARN(errno, "Cannot find user %s, falling back to %s as FTP root.",
			     FTP_DEFAULT_USER, FTP_DEFAULT_HOME);
			home = strdup(FTP_DEFAULT_HOME);
		} else {
			home = strdup(pw->pw_dir);
		}
	}

	if (!home || access(home, F_OK)) {
		ERR(errno, "Cannot access FTP root %s", home ? home : "NIL");
		return 1;
	}

	return uev_init(ctx);
}

static void ftp_cb(uev_t *w, void *arg, int events)
{
        int client;

	if (UEV_ERROR == events || UEV_HUP == events) {
		uev_io_stop(w);
		close(w->fd);
		return;
	}

        client = accept(w->fd, NULL, NULL);
        if (client < 0) {
                WARN(errno, "Failed accepting FTP client connection");
                return;
        }

        ftp_session(arg, client);
}

static void tftp_cb(uev_t *w, void *arg, int events)
{
	uev_io_stop(w);

	if (UEV_ERROR == events || UEV_HUP == events) {
		close(w->fd);
		return;
	}

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
		if (EACCES == errno)
			WARN(0, "Not allowed to start %s service.%s",
			     desc, port < 1024 ? "  Privileged port." : "");
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
	pidfile(pidfn);

	INFO("Serving files from %s ...", home);

	return uev_run(ctx, 0);
}

static char *progname(char *arg0)
{
       char *nm;

       nm = strrchr(arg0, '/');
       if (nm)
	       nm++;
       else
	       nm = arg0;

       return nm;
}

int main(int argc, char **argv)
{
	int c;
	enum {
		FTP_OPT = 0,
		TFTP_OPT,
		SEC_OPT,
		PASV_OPT
	};
	char *subopts;
	char *const token[] = {
		[FTP_OPT]  = "ftp",
		[TFTP_OPT] = "tftp",
		[SEC_OPT]  = "writable",
		[PASV_OPT] = "pasv_addr",
		NULL
	};
	uev_ctx_t ctx;
	struct in_addr in_pasv_addr;

	pidfn = prognm = progname(argv[0]);
	while ((c = getopt(argc, argv, "hl:no:p:sv")) != EOF) {
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
				case PASV_OPT:
					if (!value) {
						fprintf(stderr, "Missing PASV address argument to -o pasv_addr=ADDR");
						return usage(1);
					}
					if (!inet_aton(value,&in_pasv_addr)) {
						fprintf(stderr, "Value specified to pasv_addr is not a valid IPv4 address");
						return usage(1);
					}
					pasv_addr = strdup(value);
					break;
				case SEC_OPT:
					do_insecure = 1;
					break;

				default:
					fprintf(stderr, "Unrecognized option '%s'\n", value);
					return usage(1);
				}
			}
			break;

		case 'p':
			pidfn = optarg;
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

	if (optind < argc) {
		home = realpath(argv[optind], NULL);
		if (!home) {
			ERR(errno, "Invalid FTP root %s", argv[optind]);
			return 1;
		}
	}

	/* Inetd mode enforces foreground and syslog */
	if (string_compare(prognm, "in.tftpd")) {
		inetd      = 1;
		do_ftp     = 0;
		do_tftp    = 1;
		background = 0;
		do_syslog  = 1;
	} else if (string_compare(prognm, "in.ftpd")) {
		inetd      = 1;
		do_ftp     = 1;
		do_tftp    = 0;
		background = 0;
		do_syslog  = 1;
	}

	if (do_syslog) {
		openlog(prognm, LOG_PID | LOG_NDELAY, LOG_FTP);
		setlogmask(LOG_UPTO(loglevel));
	}

	DBG("Initializing ...");
	if (init(&ctx)) {
		ERR(0, "Failed initializing, exiting.");
		return 1;
	}

	if (inetd) {
		int sd;
		pid_t pid;

		INFO("Started from inetd, serving files from %s ...", home);

		/* Ensure socket is non-blocking */
		sd = STDIN_FILENO;
		(void)fcntl(sd, F_SETFL, fcntl(sd, F_GETFL, 0) | O_NONBLOCK);

		if (do_tftp)
			pid = tftp_session(&ctx, sd);
		else
			pid = ftp_session(&ctx, sd);

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
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
