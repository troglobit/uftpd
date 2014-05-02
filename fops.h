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

#ifndef FOPS_H_
#define FOPS_H_

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

#endif  /* FOPS_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
