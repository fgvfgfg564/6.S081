#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void writeInt(int file, int x)
{
	if(x == 0) {
		write(file, "0", 1);
		return;
	}
	char buff[32];
	int i = 0;
	while(x) {
		buff[i++] = '0' + x % 10;
		x /= 10;
	}
	i--;
	for(int j = 0; j < i - j; j++) {
		char tmp = buff[i - j];
		buff[i - j] = buff[j];
		buff[j] = tmp;
	}
	write(file, buff, i + 1);
}

int main()
{
	int p1[2], p2[2];

	pipe(p1);
	pipe(p2);
	char buff[1];
	if(fork() == 0) {
		read(p1[0], buff, 1);
		int pid = getpid();
		writeInt(1, pid);
		char msg[] = ": received ping\n";
		write(1, msg, sizeof(msg));
		write(p2[1], buff, 1);
		exit(0);
	} else {
		write(p1[1], buff, 1);
		read(p2[0], buff, 1);
		int pid = getpid();
		writeInt(1, pid);
		char msg[] = ": received pong\n";
		write(1, msg, sizeof(msg));
		exit(0);
	}
}