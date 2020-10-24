#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path)
{
	static char buf[DIRSIZ + 1];
	char *p;

	// Find first character after last slash.
	for(p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	p++;

	// Return blank-padded name.
	if(strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', DIRSIZ - strlen(p));
	return buf;
}

void *strstr(char *s, char *x)
{
	char *i, *j;
	for(i = s; *i != '\0'; ++i) {
		for(j = x; *j == *i; ++j);
		if(*j == '\0')return i;
	}
	return 0;
}

void find(char *temp, char *name)
{
	char path[512];
	memcpy(path, temp, DIRSIZ);
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;

	if((fd = open(path, 0)) < 0) {
		fprintf(2, "find: cannot open %s\n", path);
		return;
	}

	if(fstat(fd, &st) < 0) {
		fprintf(2, "find: cannot stat %s\n", path);
		close(fd);
		return;
	}

	switch(st.type) {
		case T_FILE:
			if(strstr(path, name) != 0) {
				write(1, path, sizeof(path));
				write(1, "\n", 1);
			}
			break;

		case T_DIR:
			if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
				printf("ls: path too long\n");
				break;
			}
			strcpy(buf, path);
			p = buf + strlen(buf);
			*p++ = '/';
			while(read(fd, &de, sizeof(de)) == sizeof(de)) {
				if(de.inum == 0)
					continue;
				memmove(p, de.name, DIRSIZ);
				p[DIRSIZ] = 0;
				if(stat(buf, &st) < 0) {
					printf("ls: cannot stat %s\n", buf);
					continue;
				}
				if(strcmp(fmtname(buf), ".") && strcmp(fmtname(buf), ".."))
					find(buf, name);
			}
			break;
	}
	close(fd);
}

int main(int argc, char *argv[])
{
	find(argv[1], argv[2]);
	exit(0);
}