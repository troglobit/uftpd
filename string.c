#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "string.h"

//find the first of ch in str
int _find_first_of(char *str, char ch)
{
	size_t i, len = strlen(str);

	for (i = 0; i < len; i++) {
		if (str[i] == ch)
			return i;
	}

	return -1;
}

//substring method
char *_substring(char *buf, int i, int len)
{
	int j = 0;
	int l;
	int length;
	char *dest;

	if (i < 0 || len <= 0)
		return NULL;

	l = strlen(buf);
	length = l - i > len ? len : l - i;

	dest = malloc(sizeof(char) * (length + 10));
	if (!dest)
		return NULL;

	memset(dest, 0, length + 10);
	for (j = 0; j < length; j++)
		dest[j] = buf[i + j];

	return dest;

}

//split buf with ch
int _split(char *buf, char ch, char ***result)
{
	int i = 0, first, size;
	int l = strlen(buf);
	int *loc = 0;
	int n = _all_ch_in_string(buf, ch, &loc);

	*result = (char **)malloc(sizeof(char *) * n);
	for (i = 0; i < n + 1; i++) {
		if (i == 0) {
			first = 0;
			size = loc[i];
		} else if (i == n) {
			first = loc[i - 1] + 1;
			size = l - first;
		} else {
			first = loc[i - 1] + 1;
			size = loc[i] - loc[i - 1] - 1;
		}

		(*result)[i] = _substring(buf, first, size);
	}

	if (loc)
		free(loc);

	return n + 1;
}

//return the number of ch in buf
//location storage the location of ch in buf
//location need to be free in the outer of the function
int _all_ch_in_string(char *buf, char ch, int **location)
{
	int i = 0;
	int l = strlen(buf);

	*location = (int *)malloc(sizeof(int) * l);
	int n = 0;

	for (i = 0; i < l; i++) {
		if (buf[i] == ch) {
			(*location)[n] = i;
			n++;
		}
	}

	return n;
}

//joint the array by char to one string
//example: array1 ch array2 ch array3...
char *_joint(char **array, char ch, int size)
{
	int i, j, len;
	int total = 0;
	int index = 0;
	char *result;

	for (i = 0; i < size; i++)
		total += strlen(array[i]);

	result = malloc(sizeof(char) * (total + size));
	if (!result)
		return NULL;

	result[total + size - 1] = 0;
	for (i = 0; i < size; i++) {
		len = strlen(array[i]);
		for (j = 0; j < len; j++)
			result[index++] = array[i][j];

		if (i != size - 1)
			result[index++] = ch;
	}

	return result;
}

//in this function, we won't check the input string
//ip should be free out of the function
void _transfer_str_ip_port(char *buf, char *ip[], int *port)
{
	char *ps = _substring(buf, 5, strlen(buf) - 5);
	char **array = 0;

	_split(ps, ',', &array);
	*ip = _joint(array, '.', 4);
	*port = atoi(array[4]) * 256 + atoi(array[5]);
}

//transfport ip and port to buf
//memory be released out of  the function
char *_transfer_ip_port_str(char *ip, int port)
{
	int i, j, l;
	char **ips = NULL;
	char *a, *b, *buf;
	int curIndex = 0;

	l = _split(ip, '.', &ips);
	if (l != 4) {
		perror("ip format is wrong");

	}

	buf = malloc(sizeof(char) * 30);
	if (!buf)
		return NULL;

	memset(buf, 0, sizeof(char) * 30);
	for (i = 0; i < 4; i++) {
		l = strlen(ips[i]);
		for (j = 0; j < l; j++)
			buf[curIndex++] = ips[i][j];
		buf[curIndex++] = ',';
	}

	a = parseInt2String(port / 256);
	for (i = 0; i < strlen(a); i++)
		buf[curIndex++] = a[i];
	buf[curIndex++] = ',';

	b = parseInt2String(port % 256);
	for (i = 0; i < strlen(b); i++)
		buf[curIndex++] = b[i];

	return buf;
}

/**
 * parse int to string
 */
char *parseInt2String(int num)
{
	//the length of int is 32
	char *temp = (char *)malloc(sizeof(char) * 32);

	memset(temp, 0, sizeof(char) * 32);
	int d, s, i = 0, j;

	s = num;
	while (s != 0) {
		d = s % 10;
		s = s / 10;
		temp[i++] = d + '0';
	}
	char *result = (char *)malloc(sizeof(char) * (i + 1));

	memset(result, 0, sizeof(char) * (i + 1));
	i--;
	for (j = i; j >= 0; j--) {
		result[j] = temp[i - j];
	}
	//release temp
	free(temp);
	return result;
}

int findStr(char *buf, char *str, int start)
{
	int n = strlen(buf);
	int index = start;

	for (; index < n - 2; index++) {
		if (buf[index] == str[0] &&
		    buf[index + 1] == str[1] && buf[index + 2] == str[2])
			return index;
	}

	return -1;
}

void my_strcpy(char *array, const char *array2)
{
	int l = strlen(array2);
	int i = 0;

	for (; i < l; i++)
		array[i] = array2[i];
	array[i] = 0;
}

void my_strcat(char *array, const char *array2)
{
	int l = strlen(array);

	int n = strlen(array2);
	int i;

	for (i = l; i < n + l; i++) {
		array[i] = array2[i - l];
	}
	array[i] = 0;
}
