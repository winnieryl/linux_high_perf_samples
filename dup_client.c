#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<signal.h>
#include<errno.h>

void int_handler(int sig)
{
    printf("Received SIGINT\n");
}
int main(int argc,char*argv[])
{
  if(argc<=2)
  {
    printf("usage:%s ip_address port_number\n",basename(argv[0]));
    return 1;
  }
  signal(SIGINT, int_handler);
  const char*ip=argv[1];
  int port=atoi(argv[2]);
  struct sockaddr_in server_address;
  bzero(&server_address,sizeof(server_address));
  server_address.sin_family=AF_INET;
  inet_pton(AF_INET,ip,&server_address.sin_addr);
  server_address.sin_port=htons(port);
  int sockfd=socket(PF_INET,SOCK_STREAM,0);
  assert(sockfd>=0);
  if(connect(sockfd,(struct sockaddr*)&server_address,sizeof(server_address))<0)
  {
    printf("connection failed\n");
  }
  else
  {
      char buf[128] = {0};
      printf("reading from server:\n");
      int rc;
      while(rc =read(sockfd, buf, sizeof(buf)) > 0)
      {
          printf("Receving from server:%s", buf);
      }
      if(rc == -1 && errno ==EINTR)
      {
          printf("read interupted by signal!\n");
      }
      
  }
  close(sockfd);
  return 0;
}
