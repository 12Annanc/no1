#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>



//首先上线要接收到公告



//读取服务器发来的信息
void *read_sever(void *arg)
{
	int *sockfd = (int *)arg;
	while(1)
	{
		char msg[512];
		read(*sockfd, msg, sizeof(msg));
		printf("接收到信息：[%s]\n", msg);
		//公告
		if(strstr(msg, "EOTICE"))
		{	
			char buf[4096] = {0};

			write(*sockfd, "EOTICE_OK", strlen("EOTICE_OK"));
			printf("接收到公告请求\n");
			//接收公告大小
			int eotice_size;
			int num=0;
			read(*sockfd, buf, sizeof(buf));
			sscanf( buf, "EOTICE_SIZE=[%d]", &eotice_size);
			printf("接收到文件大小[%d]\n", eotice_size);

			write(*sockfd, "SIZE_OK", strlen("SIZE_OK"));

			while(num != eotice_size)
			{
				char eotice_buf[1024];
				int rev = read(*sockfd, eotice_buf, sizeof(eotice_buf));
				num += rev;
				printf("收到公告内容为：[%s]\n", eotice_buf);
			}
		}
	}
}


//向服务器发送信息
void *write_sever(void *arg)
{
    int *new_socket = (int*)arg;

    while(1)
    {
        char buf[1024] = {0};
        scanf("%s", buf);
        write(*new_socket, buf, strlen(buf)); 
		printf("发送成功%s\n", buf);   
    }
	

}





int main(int argc,char *argv[])  //./client 192.168.22.xx 50002
{	
	if(argc != 3)
	{
		printf("./程序名  ip  port");
		return 0;
	}
	
	int ret;
	char buf[1024] = {0};	

	while(1)
	{

		//1. 创建TCP套接字
		int sockfd;
		sockfd = socket(AF_INET,SOCK_STREAM,0);		
		//2 准备服务器的结构体。
		struct sockaddr_in srvaddr;
		
		bzero(&srvaddr,sizeof(srvaddr));
		printf("================\n");
		printf("1.居民列表（可聊天）\n");
		printf("2.生活助手\n");
		printf("================\n");
		
		printf("请选择选项：\n");

		int sign;
		scanf("%d",&sign);
		

		switch(sign)
		{
			case 1:
				srvaddr.sin_family = AF_INET;//IPV4
				srvaddr.sin_port = htons(atoi(argv[2]));//端口号
				srvaddr.sin_addr.s_addr = inet_addr(argv[1]);//设置好服务器的地址
				
				int reval = connect(sockfd,(struct sockaddr *)&srvaddr,sizeof(srvaddr));
				if(reval > 0)
				{
					perror("");
					return 0;
				}
				
				printf("你好啊\n");
				pthread_t read_tid, write_tid;
				pthread_create(&read_tid, NULL, read_sever, (void *)&sockfd);
				pthread_create(&write_tid, NULL, write_sever, (void *)&sockfd);

				pthread_join(read_tid, NULL);
				pthread_join(write_tid, NULL);


				printf("通信退出\n");
				close(sockfd);

			break;
			
			case 2:
			    //设置服务器的IP地址信息

				srvaddr.sin_family  = AF_INET;
				srvaddr.sin_port   = htons(80);
				srvaddr.sin_addr.s_addr = inet_addr("47.107.155.132");
				
				int ret=connect(sockfd,(struct sockaddr *)&srvaddr,sizeof(srvaddr));
				if(ret<0)
				{
					perror("");
					return 0;
				}
				else{
					printf("链接服务器成功！\n");
				}
				
				//重点！！定制HTTP 请求协议  
				//https://    cloud.qqshabi.cn    /api/hitokoto/hitokoto.php 
				char *http="GET /api.php?key=free&appid=0&msg=天气广州 HTTP/1.1\r\nHost:api.qingyunke.com\r\n\r\n";
				
				//发送数据给服务器
				write(sockfd,http,strlen(http));
				
				//获取服务器的信息
				char buf[4096]={0};
				int size=read(sockfd,buf,4096);
				
				printf("size =%d  buf =%s\n",size,buf);
				
				close(sockfd);

			break;

			default:
				printf("没有该选项！\n");
			break;
		}
	}
	return 0;
}


/*
经过多次试验，这里不能跟服务器端那样直接在后面使用connfd
“添加集合” 和 “判断最大套接字” 以及 “发送数据” 和 “接收数据” 
要使用sockfd，不能使用connfd,否则无法收发数据

*/