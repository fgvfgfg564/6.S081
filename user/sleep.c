#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
	char err[] = "Please enter the number of seconds to sleep.\n";
	if(argc == 1){
		write(2, err, sizeof(err));
		exit(0);
	}
	sleep(atoi(argv[1]));
	exit(0);
}