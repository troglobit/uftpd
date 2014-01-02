#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ftpserver.h"
int main(int argc, char**argv)
{
    struct FtpServer *s = (struct FtpServer*) malloc(sizeof(struct FtpServer));
    if(argc >= 2)
    {
    	show_log(argv[1]);
    s->_port = atoi(argv[2]);
    }
    else
    {
    	 s->_port = 21;
    }
    if(argc < 3)
    //strcpy(s->_relative_path, argv[2]);
    {
    	strcpy(s->_relative_path, "/tmp");
    }
    else
    {
    	show_log(argv[2]);
    	 strcpy(s->_relative_path, argv[4]);
    }
    init_ftp_server(s);
    start_ftp_server(s);
   // close_ftp_server(s);
    return 0;
}
