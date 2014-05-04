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

#ifndef UFTPD_H_
#define UFTPD_H_

#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <locale.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "defs.h"
#include "ftpcmd.h"
#include "string.h"

#define FTP_DEFAULT_PORT  21
#define FTP_DEFAULT_USER  "ftp"
#define FTP_SERVICE_NAME  "ftp"
#define FTP_PROTO_NAME    "tcp"
#define FTP_DEFAULT_HOME  "/srv/ftp"

/* XXX: What's a "good" buffer size, 4096? */
#define BUFFER_SIZE       1000

/* This is a stupid server, it doesn't expect >20 sec inactivity */
#define INACTIVITY_TIMER  20

#ifndef UNUSED
#define UNUSED(x) UNUSED_ ## x __attribute__ ((unused))
#endif

#define SETSIG(sa, sig, fun, flags)			\
	do {						\
		sa.sa_sigaction = fun;			\
		sa.sa_flags = SA_SIGINFO | flags;	\
		sigemptyset(&sa.sa_mask);		\
		sigaction(sig, &sa, NULL);		\
	} while (0)

#define ERR(code, fmt, args...)  logit(LOG_ERR, code, fmt, ##args)
#define WARN(code, fmt, args...) logit(LOG_WARNING, code, fmt, ##args)
#define LOG(fmt, args...)        logit(LOG_NOTICE, 0, fmt, ##args)
#define INFO(fmt, args...)       do { if (verbose) logit(LOG_INFO, 0, fmt, ##args);  } while(0)
#define DBG(fmt, args...)        do { if (debug)   logit(LOG_DEBUG, 0, fmt, ##args); } while(0)
#define show_log(msg)            DBG(msg)

extern char *__progname;
extern int   port;              /* Server listening port            */
extern char *home;		/* Server root/home directory       */
extern char  inetd;             /* Bool: conflicts with daemonize   */
extern char  background;	/* Bool: conflicts with inetd       */
extern char  debug;             /* Level: 1-7, only 1 implemented   */
extern char  verbose;           /* Bool: Enables extra logging info */
extern char  do_log;            /* Bool: False at daemon start      */
extern char *logfile;           /* Logfile, when NULL --> syslog    */
extern struct passwd *pw;       /* FTP user's passwd entry          */

void collect_sessions(void);
void logit(int severity, int code, const char *fmt, ...);

#endif  /* UFTPD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
