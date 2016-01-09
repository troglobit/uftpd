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

#ifndef UFTPD_H_
#define UFTPD_H_

#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/tftp.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>		/*  PRIu64/PRI64, etc. for stdint.h types */
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>		/* isset(), setbit(), etc. */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <libuev/uev.h>
#include <libite/lite.h>

#define FTP_DEFAULT_PORT  21
#define FTP_SERVICE_NAME  "ftp"
#define FTP_PROTO_NAME    "tcp"

#define TFTP_DEFAULT_PORT 69
#define TFTP_SERVICE_NAME "tftp"
#define TFTP_PROTO_NAME   "udp"

#define FTP_DEFAULT_USER  "ftp"
#define FTP_DEFAULT_HOME  "/srv/ftp"

/* XXX: What's a "good" buffer size, 4096? */
#define BUFFER_SIZE       1000

/* This is a stupid server, it doesn't expect >20 sec inactivity */
#define INACTIVITY_TIMER  20 * 1000

/* TFTP Packet Types (New) */
#define OACK              06	/* option acknowledgement */

/* TFTP Minimum segment size, specific to uftpd */
#define MIN_SEGSIZE       32

#define LOGIT(severity, code, fmt, args...)				\
	do {								\
		if (code)						\
			logit(severity, fmt ". Error %d: %s%s",		\
			      ##args, code, strerror(code),		\
			      do_syslog ? "" : "\n");			\
		else							\
			logit(severity, fmt "%s", ##args,		\
			      do_syslog ? "" : "\n");			\
	} while (0)
#define ERR(code, fmt, args...)  LOGIT(LOG_ERR, code, fmt, ##args)
#define WARN(code, fmt, args...) LOGIT(LOG_WARNING, code, fmt, ##args)
#define LOG(fmt, args...)        LOGIT(LOG_NOTICE, 0, fmt, ##args)
#define INFO(fmt, args...)       do { if (debug || verbose) LOGIT(LOG_INFO, 0, fmt, ##args);  } while(0)
#define DBG(fmt, args...)        do { if (debug)            LOGIT(LOG_DEBUG, 0, fmt, ##args); } while(0)

extern char *__progname;
extern char *home;		/* Server root/home directory       */
extern int   inetd;             /* Bool: conflicts with daemonize   */
extern int   background;	/* Bool: conflicts with inetd       */
extern int   chrooted;		/* Bool: are we chrooted?           */
extern int   debug;             /* Level: 1-7, only 1 implemented   */
extern int   verbose;           /* Bool: Enables extra logging info */
extern int   do_syslog;         /* Bool: False at daemon start      */
extern int   do_ftp;            /* Port: FTP port, or disabled      */
extern int   do_tftp;           /* Port: TFTP port, or disabled     */
extern struct passwd *pw;       /* FTP user's passwd entry          */

typedef struct tftphdr tftp_t;

typedef struct {
	int sd;
	int type;

	char cwd[PATH_MAX];

	struct sockaddr_storage server_sa;
	struct sockaddr_storage client_sa;

	char serveraddr[INET_ADDRSTRLEN];
	char clientaddr[INET_ADDRSTRLEN];

	/* Event loop context and session watchers */
	uev_t      io_watcher, timeout_watcher;
	uev_ctx_t *ctx;

	/* Session buffer */
	char    *buf;		/* Pointer to segment buffer */
	size_t   bufsz;		/* Size of buf */

	/* TFTP */
	char    *file;	        /* Current file name to fetch */
	FILE    *fp;		/* Current file in operation */
	tftp_t  *th;		/* Same as buf, only as tftp_t */
	size_t   segsize;	/* SEGSIZE, or per session negotiated */
	int      timeout;	/* INACTIVITY_TIMER, or per session neg. */
	uint32_t tftp_options;	/* %1:blksize */

	/* User credentials */
	char name[20];
	char pass[20];

	/* PASV */
	int data_sd;
	int data_listen_sd;

	/* PORT */
	char data_address[INET_ADDRSTRLEN];
	int  data_port;
} ctrl_t;

ctrl_t *new_session(uev_ctx_t *ctx, int sd, int *rc);
int     del_session(ctrl_t *ctrl, int isftp);

int     ftp_session(uev_ctx_t *ctx, int client);
int     tftp_session(uev_ctx_t *ctx, int client);

char   *compose_path(ctrl_t *ctrl, char *path);
int     open_socket(int port, int type, char *desc);
void    convert_address(struct sockaddr_storage *ss, char *buf, size_t len);

void    logit(int severity, const char *fmt, ...);

#endif  /* UFTPD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
