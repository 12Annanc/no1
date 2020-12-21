#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "font.h"



#define LIST   1
#define TALK   2
#define BACK   3


pthread_cond_t cond, cond1;
pthread_mutex_t mutex;

//首先上线要接收到公告
pthread_t read_tid, write_tid;


//============================LCD=================
//===============lcd=================================================
struct LcdDevice *init_lcd(const char *device)
{
	//申请空间
	struct LcdDevice* lcd = malloc(sizeof(struct LcdDevice));
	if(lcd == NULL)
	{
		return NULL;
	} 

	//1打开设备
	lcd->fd = open(device, O_RDWR);
	if(lcd->fd < 0)
	{
		perror("open lcd fail");
		free(lcd);
		return NULL;
	}
	
	//映射
	lcd->mp = mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd->fd,0);

	return lcd;
}
//===============================================================================
void draw(char *buf)
{
	//初始化Lcd
	struct LcdDevice* lcd = init_lcd("/dev/fb0");

	//打开字体	
	font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");
	
	//字体大小的设置
	fontSetSize(f,30); 				
	
	//创建一个画板
	bitmap *bm = createBitmapWithInit(800,480,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色		
	//将字体写到点阵图上
	fontPrint(f,bm,50,30,buf,getColor(0,255,0,0),700);
					
	//把字体框输出到LCD屏幕上
	show_font_to_lcd(lcd->mp,0,0,bm);				

	//关闭字体，关闭画板
	destroyBitmap(bm);//画板需要每次都销毁 
}




//================================================


//发送文件
int send_file(int *sockfd)
{
	char filename[512];
	char sendname[512];

	bzero(filename, 512);
	bzero(sendname, 512);
	//发送文件名，打开本地文件
	//文件名为本地文件名
	printf("请输入你要发送的文件位置[绝对路径]：");
	scanf("%s", filename);

	printf("[%s]\n", filename);

	draw(filename);

	
	//确保文件路径没问题
	if(strstr(filename, "/"))
	{
		strcpy(sendname, strrchr(filename, '/')+1);
		printf("要发送的文件名字为[%s], sendname =%s\n", filename, sendname);
				
	}
	else
	{
		char p[1024] = {0};
		sprintf(p, "./%s", filename);
		strcpy(sendname, strrchr(p, '/')+1);
		printf("拼接之后的名字\n");
	}

	//打开文件
    int fd = open(filename, O_RDONLY);
    if(fd == -1)
    {
        perror("open file error");
        return 0;
    }
	
    int file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    printf("要发送的文件的大小为%d个字节\n", file_size);
	char  head[1024]={0};
    sprintf(head, "[FILE][%d]%s", file_size, sendname);

	//吧验证信息和文件大小发送过去s
	write(*sockfd, head, strlen(head));

	//等待对方回应  
	bzero(head,1024);
	read(*sockfd,head,1024);	
				
	if(strstr(head,"OK"))  //对方成功接收到 文件大小
	{
		
		int  send_size=0;
		while(1)
		{
			//读取文件中的数据  
			char send[4096]={0};
			int size=read(fd,send,4096);
			
			printf("发送的大小 %d 总大小 %d\n",size,send_size);
			
			//发送网络中 
			int  s_size=write(*sockfd,send,size);
			send_size+=s_size;
			if(send_size == file_size)
			{
				printf("发送完毕等待对方接收完毕\n");
				draw("请输入你要发送的信息[exit退出]");
				bzero(head,1024);
				read(*sockfd,head,1024);
				if(strstr(head,"END"))
				{
					printf("文件传输完毕\n");
					break;
				}
			}
		}
	}
}


//聊天文件服务函数
int talk_sever(int *sockfd)
{

	//和服务器进行沟通,判断是要发送文件还是要发送消息
	//还要判断是否有留言和文件
	//还要判断是什么时候发过来的，我在聊天时发过来要立即接收，不在聊天时要存放在服务器和留言中
	//聊天时只能接收文件不能发送文件


	while(1)
	{
		int sign = 0;
		printf("1.发送信息  2.发送文件, 3.退出\n");
		draw("1.发送信息  2.发送文件, 3.退出");

		scanf("%d", &sign);

		
		switch (sign)
		{
			case 1:
				printf("请输入你要发送的信息[exit退出]\n");
				while(1)
				{
					fd_set set;
					FD_ZERO(&set);

					int max = *sockfd;

					FD_SET(*sockfd, &set);
					FD_SET(0, &set);

					int rev = select((max)+1, &set, NULL, NULL, NULL);
					if(rev < 0)
					{
						perror("select");
						return 0;
					}

					//收到客户端发过来的信息
					if(FD_ISSET(*sockfd, &set))
					{
						char buf[4096] = {0};
						
						int rev_red = read(*sockfd, buf, 4096);
						if(rev_red <= 0)
						{
							perror("服务器已断线\n");
							return 0;
						} 
						//对收到的信息进行判断是否是文件
						if(strstr(buf, "FILE"))
						{
							printf("接收到文件信息:%s\n", buf);
							int filesize;

							char filename[64] = {0};

							//截取有用信息
							sscanf( buf, "[FILE]大小为[%d]文件%s",&filesize,filename);
					
							printf("对方发送文件是否接收 1 YES\n");
							draw("对方发送文件是否接收 1 YES\n");
						
							int a=0;
							scanf("%d",&a);
							
							if(a)  //接收文件 
							{
								char file_name[1024]={0};
								printf("请输入另存为名\n");
								draw("请输入另存为名");
								scanf("%s",file_name);

								int fd=open(file_name,O_RDWR|O_CREAT|O_TRUNC,0777);
								if(fd < 0)
								{
									perror("接收失败");
									return 0;
								}

								write(*sockfd, "OK", strlen("OK"));

								//接收网络数据写入到本地文件中 
								int  recv_size=0;
								while(1)
								{
									char   recv[4096]={0}; 
									int size = read(*sockfd,recv,4096); //读取网络数据 
									recv_size += size;
									printf("当前接收的大小 %d ,总大小 %d\n",size,recv_size);
									write(fd,recv,size); //写入到本地文件中  
									
									if(recv_size == filesize)
									{
										printf("接收完毕\n");
										draw("接收完毕");
											//应答对方 
										write(*sockfd,"END",strlen("END"));
										close(fd); 
										break;
									}
								}
							}
						}
						else
						{
							//打印收到的信息
							printf("%s\n", buf);
							draw(buf);
						}
					}

					if(FD_ISSET(0, &set))  
					{
						//给服务器发送聊天信息[TALK]
						char buf[4000] = {0};
						char new_buf[4096] = {0};
						
						int rev = read(0, buf, 4000);
						if(strstr(buf, "exit"))
						{
							printf("退出聊天\n");
							
							break;
						}

						sprintf( new_buf, "[TALK]%s", buf);

						write(*sockfd, new_buf, strlen(new_buf));
						printf("发送成功[exit退出]\n");
					}  
				} 			
				break;
			
			case 2:
				send_file(sockfd);
				break;
			case 3:
				write(*sockfd, "EXIT", strlen("EXIT"));
				return 0;

			default:
				printf("请选择正确的功能\n");
				draw("请选择正确的功能");
				break;
		}
	}

}



//读取服务器发来的信息
void *read_sever(void *arg)
{
	int *sockfd = (int *)arg;
	while(1)
	{
		char msg[512] = {0};
		int rev_read = read(*sockfd, msg, sizeof(msg));
		if(rev_read <=0 )
		{
			perror("服务器已下线");
			pthread_cancel(write_tid);
			return NULL;
		}
		//printf("接收到信息：[%s]\n", msg);

		//公告
		if(strstr(msg, "EOTICE"))
		{	
			char buf[4096] = {0};

			write(*sockfd, "OK", strlen("OK"));
			//printf("接收到公告请求\n");
			//接收公告大小
			int eotice_size;
			int num=0;
			read(*sockfd, buf, sizeof(buf));
			sscanf( buf, "FILE_SIZE=[%d]", &eotice_size);
			//printf("接收到文件大小[%d]\n", eotice_size);

			//反馈已经接收到文件大小
			write(*sockfd, "SIZE_OK", strlen("SIZE_OK"));
			char buf1[1024];
			while(num != eotice_size)
			{
				char eotice_buf[1024];
				int rev = read(*sockfd, eotice_buf, sizeof(eotice_buf));
				num += rev;
				printf("收到公告内容为：[%s]\n", eotice_buf);
				sprintf(buf1,"社区公告：%s",eotice_buf);
				//公告

				draw(buf1);	
				sleep(3);
				pthread_cond_signal(&cond);	
			}
		}

		//打印用户列表
		if(strstr(msg, "MAN_LIST"))
		{
			char buf[4096] = {0};

			write(*sockfd, "OK", strlen("OK"));

			//接收公告大小
			int eotice_size;
			int num=0;
			read(*sockfd, buf, sizeof(buf));
			sscanf( buf, "FILE_SIZE=[%d]", &eotice_size);
			//printf("接收到文件大小[%d]\n", eotice_size);

			write(*sockfd, "SIZE_OK", strlen("SIZE_OK"));

			while(num != eotice_size)
			{
				char eotice_buf[1024];
				int rev = read(*sockfd, eotice_buf, sizeof(eotice_buf));
				num += rev;
				//printf("%s\n", eotice_buf);
				//用户列表
				while(1)
				{
					draw(eotice_buf);
					char p[100] = {0};
					printf("输入任意内容退出\n");
					scanf("%s", p);
					pthread_cond_signal(&cond);
					break;					
				}
			}

		}

		//用户要找人聊天
		if(strstr(msg, "TALK_OTHER"))
		{
			while(1)
			{
				//用于判断是否退出聊天
				int sign = 0;

				char othre_ip[512];
				printf("请输入你要聊天的对象[ip]:\n");
				scanf("%s", othre_ip);

				//printf("要发送的ip[%s]\n", othre_ip);
				
				write(*sockfd, othre_ip, strlen(othre_ip));
				while(1)
				{
					char buf[64] = {0};
					int rev = read(*sockfd, buf, sizeof(buf));
					
					if(rev == 3)
					{	
						printf("我已经接收到%s\n", buf);
						//和服务器聊天
						talk_sever(sockfd);
						sign = 1;
						break;
					}
					else
					{
						printf("ip输入有误, 请重新输入\n");
						break;
					}
				}
				if(sign == 1)
				{
					break;
				}				
			}
			pthread_cond_signal(&cond1);
		}
	}
}


//向服务器发送信息
void *write_sever(void *arg)
{
    int *new_socket = (int*)arg;

	pthread_cond_wait(&cond, &mutex);

    while(1)
    {
		//用户之间的交流
		//1.查看在线列表  2.选择好友发送信息  3.选择好友发送文件  4.退出
		int sign;
		printf("请选择功能：1.查看在线列表  2.选择好友发送信息/文件   3.退出\n");
//==============================================================		
			//初始化Lcd
		struct LcdDevice* lcd = init_lcd("/dev/fb0");

		//打开字体	
		font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");
		
		//字体大小的设置
		fontSetSize(f,30); 				
		
		//创建一个画板
		bitmap *bm = createBitmapWithInit(800,480,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色		
		//将字体写到点阵图上
		fontPrint(f,bm,50,30,"1.查看在线列表",getColor(0,255,0,0),700);
		fontPrint(f,bm,50,70,"2.选择好友发送信息/文件 ",getColor(0,255,0,0),700);
		fontPrint(f,bm,50,110,"3.退出",getColor(0,255,0,0),700);
		
		//把字体框输出到LCD屏幕上
		show_font_to_lcd(lcd->mp,0,0,bm);				

		//关闭字体，关闭画板
		destroyBitmap(bm);//画板需要每次都销毁		
//===============================================================
		scanf("%d", &sign);

		switch(sign)
		{
			//显示列表
			case LIST:
				write(*new_socket, "LIST", strlen("LIST"));
				pthread_cond_wait(&cond, &mutex);
				break;
			//和别人聊天
			case TALK:
				write(*new_socket, "TALK", strlen("TALK"));
				pthread_cond_wait(&cond1, &mutex);
				break;

			default:
				break;

			case BACK:
				goto home;
				break;
		}
    }
home:
	pthread_cancel(read_tid);
}


int assi(int sockfd,struct sockaddr_in srvaddr)
{

	draw("例：输入广州天气或笑话");//开发板显示
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
		printf("===============\n");
	}
	char cont[1024];
	printf("请输入要内容：");
	scanf("%s",cont);
	char http[1204];

	sprintf(http,"GET /api.php?key=free&appid=0&msg=%s HTTP/1.1\r\nHost:api.qingyunke.com\r\n\r\n",cont); 
	//char *http="GET /api.php?key=free&appid=0&msg=天气上饶 HTTP/1.1\r\nHost:api.qingyunke.com\r\n\r\n";

	//发送数据给服务器
	write(sockfd,http,strlen(http));
	
	//获取服务器的信息
	char buf[4096]={0};
	read(sockfd,buf,4096);
	
	//指向文件末尾
	char *end = strstr(buf,"\r\n\r\n")+31;
	
	printf("%s",end);//终端显示
	draw(end);//开发板显示

	printf("输入任意字符退出\n");
	char arg[100] = {0};
	scanf("%s", arg);
		
	close(sockfd);
	return 0;
}




int main(int argc,char *argv[])
{	
	if(argc != 3)
	{
		printf("./程序名  服务器ip  服务器port\n");
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
		printf("1.居民互动\n");
		printf("2.生活助手\n");
		printf("3.退出\n");
		printf("================\n");
		
		printf("请选择选项：\n");

//====================================================
			//初始化Lcd
			struct LcdDevice* lcd = init_lcd("/dev/fb0");

			//打开字体	
			font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");
			
			//字体大小的设置
			fontSetSize(f,30); 				
			
			//创建一个画板
			bitmap *bm = createBitmapWithInit(800,480,4,getColor(0,255,255,255)); //也可使用createBitmapWithInit函数，改变画板颜色		
			//将字体写到点阵图上
			fontPrint(f,bm,50,30,"1.居民互动",getColor(0,255,0,0),700);
			fontPrint(f,bm,50,70,"2.生活助手",getColor(0,255,0,0),700);
			fontPrint(f,bm,50,110,"3.退出",getColor(0,255,0,0),700);
			
			//把字体框输出到LCD屏幕上
			show_font_to_lcd(lcd->mp,0,0,bm);				

			//关闭字体，关闭画板
			destroyBitmap(bm);//画板需要每次都销毁  
//====================================================
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
				
				pthread_create(&read_tid, NULL, read_sever, (void *)&sockfd);
				pthread_create(&write_tid, NULL, write_sever, (void *)&sockfd);

				pthread_join(read_tid, NULL);
				pthread_join(write_tid, NULL);

				printf("通信退出\n");
				close(sockfd);

			break;
			
			case 2:
				assi(sockfd,srvaddr);
			break;

			case 3:
				return 0;

			default:
				printf("没有该选项！\n");
			break;
		}
	}
	return 0;
}

