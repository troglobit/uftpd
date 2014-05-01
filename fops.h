#ifndef FOPS_H_
#define FOPS_H_

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

int _get_file_size(int file)
{
	int size;

	size = lseek(file, 0L, SEEK_END);
	lseek(file, 0L, SEEK_SET);

	return size;
}

int _get_file_size2(FILE * file)
{
	int size;

	fseek(file, 0L, SEEK_END);
	size = ftell(file);
	if (size < 0)
		perror("file error");
	fseek(file, 0L, SEEK_SET);

	return size;
}

int is_exist_dir(char *_dir)
{
	DIR *dir;

	dir = opendir(_dir);
	if (!dir) {
		closedir(dir);
		return 0;
	}

	closedir(dir);
	return 1;
}

#endif				/* FOPS_H_ */
