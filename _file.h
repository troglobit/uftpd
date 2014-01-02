#ifndef _FILE_H_
#define _FILE_H_
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
//return file size
int _get_file_size(int file)
{
	int size = lseek(file, 0L, SEEK_END);
	lseek(file, 0L, SEEK_SET);
	return size;
}
 int _get_file_size2(FILE* file)
 {
	 fseek(file, 0L, SEEK_END);
	 int size = ftell(file);
	 if(size < 0)
	 {
		 perror("file error.");
	 }
	 fseek(file, 0L, SEEK_SET);
	 return size;
 }
 int is_exist_dir(char* _dir)
 {
	 DIR * dir = NULL;
	 dir = opendir(_dir);
     if(dir == NULL)
     {
    	 closedir(dir);
    	 return 0;
     }
     else
     {
    	 closedir(dir);
    	 return 1;
     }

 }
#endif
