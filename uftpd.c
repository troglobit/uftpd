#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ftpcmd.h"

int main(int argc, char **argv)
{
	struct FtpServer *ctx = malloc(sizeof(struct FtpServer));

        if (!ctx) {
                perror("Out of memory");
                return 1;
        }

	if (argc >= 2) {
		show_log(argv[1]);
		ctx->_port = atoi(argv[2]);
	} else {
		ctx->_port = 21;
	}

	if (argc < 3) {
		strcpy(ctx->_relative_path, "/srv/ftp");
	} else {
		show_log(argv[2]);
		strcpy(ctx->_relative_path, argv[4]);
	}

	init_ftp_server(ctx);
	start_ftp_server(ctx);
	// close_ftp_server(ctx);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
