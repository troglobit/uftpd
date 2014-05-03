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

/* Global daemon settings */
int   port        = FTP_DEFAULT_PORT;
char *home        = NULL;
char  inetd       = 0;
char  daemonize   = 0;
char  debug       = 0;
char  verbose     = 0;
char  do_log      = 0;
char *logfile     = NULL;
struct passwd *pw = NULL;

static int version(void)
{
	printf("Version %s\n", VERSION);
	return 0;
}

static int usage(void)
{
	printf("\nUsage: %s [-d | -i] [-D] [-V] [-l LOGFILE] [-p PORT]\n"
	       "\n"
	       "  -h | -?  Show this help text\n"
	       "  -d       Daemonize, run in background\n"
	       "  -D       Enable developer debug logs\n"
	       "  -i       Inetd mode, take client connections from stdin\n"
	       "  -l PATH  Full path to logfile, otherwise syslog is used\n"
	       "  -p PORT  Port to serve files on, default %d\n"
	       "  -v       Show program version\n"
	       "  -V       Verbose logging\n"
	       "\n"
	       "Bug report address: %-40s\n\n", __progname, FTP_DEFAULT_PORT, BUGADDR);

	return 0;
}

static void init(void)
{
	struct servent *sv;

	sv = getservbyname(FTP_SERVICE_NAME, FTP_PROTO_NAME);
	if (!sv) {
		port = FTP_DEFAULT_PORT;
		WARN(errno, "Cannot find service %s/%s, falling back to port %d.",
		     FTP_SERVICE_NAME, FTP_PROTO_NAME, ntohs(port));
	} else {
		port = ntohs(sv->s_port);
	}

	pw = getpwnam(FTP_DEFAULT_USER);
	if (!pw) {
		home = strdup(FTP_DEFAULT_HOME);
		WARN(errno, "Cannot find user %s, falling back to %s as FTP root.",
		     FTP_DEFAULT_USER, home);
	} else {
		home = strdup(pw->pw_dir);
	}
}

int main(int argc, char **argv)
{
	int c;

	init();

	while ((c = getopt (argc, argv, "h?dDil:p:vV")) != EOF) {
		switch (c) {
		case 'd':
			if (inetd)
				WARN(0, "-i already given, cannot daemonize %s.", __progname);
			else
				daemonize = 1;
			break;

		case 'D':
			debug = 1;
			break;

		case 'i':
			if (daemonize)
				WARN(0, "-d already given, cannot start %s in inetd mode.", __progname);
			else
				inetd = 1;
			break;

		case 'l':
			logfile = strdup(optarg);
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 'v':
			return version();

		case 'V':
			verbose = 1;
			break;

		default:
			return usage();
		}
	}

	if (!logfile) {
		do_log = 1;
		openlog (__progname, LOG_PID | LOG_NDELAY, LOG_FTP);
	}

	if (inetd) {
		LOG("Started from inetd, serving files from %s ...", home);
		return start_session(STDIN_FILENO);
	}

	return serve_files();
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
