#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

char buf[512];
char *argv2[MAXARG];
char argv_buff[MAXARG][512];

void show(char *strs[])
{
	for (int i = 0; strs[i]; i++) {
		write(1, strs[i], strlen(strs[i]));
		write(1, "\n", 1);
	}
}

int main(int argc, char *argv[])
{
	char c;
	for (int i = 1; i < argc; i++)argv2[i - 1] = argv[i];
	while (1) {
		int len = 0;
		int stat;
		while ((stat = read(0, &c, 1)) && c != '\n')buf[len++] = c;
		if (!len)break;
		int p = 0;
		int lst = 0;
		for (int i = 0; i <= len; i++) {
			if (i == len || buf[i] == ' ') {
				memcpy(argv_buff[p], buf + lst, i - lst);
				memset(argv_buff[p] + i - lst, '\0', 512 - (i - lst));
				argv2[p + argc - 1] = argv_buff[p];
				p++;
				lst = i;
			}
		}
		argv2[p + argc - 1] = 0;
		int pid;
		if ((pid = fork()) == 0) {
			exec(argv[1], argv2);
			exit(0);
		} else
			wait(0);
		if (!stat)break;
	}
	exit(0);
}