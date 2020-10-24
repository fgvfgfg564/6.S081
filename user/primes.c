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

void process(int input)
{
	char buff;
	int i = 0;
	int p[2];
	int prime = 1;
	while(read(input, &buff, 1)) {
		i++;
		if(i == 1) {
			write(1, "prime ", 6);
			writeInt(1, buff);
			prime = buff;
			write(1, "\n", 1);
		}
		if(i == 2) {
			pipe(p);
			if(fork() == 0) {
				close(input);
				close(p[1]);
				process(p[0]);
				exit(0);
			} else close(p[0]);
		}
		if(i >= 2 && buff % prime != 0)write(p[1], &buff, 1);
	}
	if(i > 1)close(p[1]);
	close(input);
}

int main()
{
	int p[2];
	pipe(p);
	if(fork() != 0) {
		close(p[0]);
		for(int i = 2; i <= 35; i++)
			write(p[1], &i, 1);
		close(p[1]);
		exit(0);
	} else {
		close(p[1]);
		process(p[0]);
		exit(0);
	}
}