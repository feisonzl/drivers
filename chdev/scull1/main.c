#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
int main()
{
	int fd,ret;
	char buf[100]={0};
	fd = open("/dev/scull0",O_RDWR);			
    ret=write(fd, "scull0 write test\n",sizeof("scull0 write test\n"));
	printf("write %d char!\n",ret);
	ret=read(fd,buf,10);
	printf("read test:%s ret:%d\n",buf,ret);
	return 0;
}
