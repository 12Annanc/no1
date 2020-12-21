/*
    服务器

    可以再访问服务器的同时定义新的socket访问其他服务器

    功能：利用链表将用户信息存放起来，一个用户一个线程
          每次有用户连接上来就发送社区公告
          查询在线用户
          指定用户进行聊天

          已经可以指定用户发送信息，但还没有处理留言， 和输入推出

          查看列表，发送信息和退出都已经完成了，现在缺少的是发送文件和图片，还有公告没有具体的确定是什么类型

          2020年12月19日10:37:21
          发送文件和图片已经完成功能应该没什么问题，目前还没发现什么问题，现在还缺少的就是上开发板测试
*/

#include <stdio.h>
#include <sys/types.h>       
#include <sys/socket.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define IN_FREE 10
#define IN_LIST 20
#define IN_TALK 30


int people_num = 0;
char LOCAT[100] = {0};
char NOTICE[4096] = {0};


//安全的向上遍历（允许你在for循环当中任意操作pos节点而不影响链表的遍历）
#define list_for_each_safe_tail(head, pos, n)\
    for(pos=head->prev, n=pos->prev; pos != head; pos=n, n=n->prev)
    //安全的向下遍历（允许你在for循环当中任意操作pos节点而不影响链表的遍历）
#define list_for_each_safe(head, pos, n)\
    for(pos=head->next, n=pos->next; pos != head;  pos=n, n=n->next)


//存放用户信息
struct people
{
    int sta;//用户状态
    int socket;  	
	struct sockaddr_in  addr;

    int leave_sign;//记录是否有留言
    char leave[4096];

    int file_sign;//记录是否有文件
    char filename[1024];//文件名
};


typedef struct link_list
{
    struct people msg;
    struct link_list *list_head;

    struct link_list *next;
    struct link_list *prev;
}list_node;


//新节点的申请及初始化
list_node *request_list_node(struct sockaddr_in clien_addr, int socket, list_node *list_head)
{
    list_node *new_node;

    new_node = malloc(sizeof(list_node));
    if(new_node == NULL)
    {
        perror("申请内存失败");
        return NULL;
    }
    
    //如果不是头节点
    if(socket != -1)
    {
        new_node->list_head = list_head;
        new_node->msg.addr = clien_addr;
        new_node->msg.socket = socket;
    }

    new_node->prev = new_node;//新节点的前一个和后一个都指向自己
    new_node->next = new_node;

    return new_node;
}

//新结点插在头结点前面
void insert_node_to_list_tail(list_node *head, list_node *insert_node)
{
    insert_node->next = head;//插入节点的下一个指向head
    insert_node->prev = head->prev;//插入节点的上一个指向head的上一个
    head->prev->next = insert_node;//让head的上一个节点的下一个位置指向插入的节点
    head->prev = insert_node;//让head的上一个指向插入的节点
}


//遍历链表
void display_list_node(list_node *list_head)
{
    list_node *pos, *n;
    printf("链表的数据：\n");
    list_for_each_safe_tail(list_head, pos, n)
    {

        char  *ip  = inet_ntoa(pos->msg.addr.sin_addr);  //获取对方地址
        printf("ip :%s    socket :%d\n", ip, pos->msg.socket);

    }
    printf("\n______________________\n");

}

//将指定的节点移除出表格
void remove_list_node(list_node *rm_node)
{
    list_node* p = rm_node->prev;
    list_node* q = rm_node->next;
    p->next = q;
    q->prev = p;
}

//销毁链表
void destroy_list(list_node *list_head)
{
    list_node *pos, *n;

    list_for_each_safe(list_head, pos, n)
    {
        char  *ip  = inet_ntoa(pos->msg.addr.sin_addr);  //获取对方地址
        printf("free %s\n", ip);
        free(pos);
    }

    free(list_head);
}


//判断对方是否最备好连接
int link_jodug(int socket, int file_size, char *p)
{

    char buf[512] = {0};
    char mark[512] = {0};

    strcpy(mark, p);
    //发送标志位
    write(socket, mark, strlen(mark));
    
    while(1)
    {
        char sign[512] = {0};
        int rev = read(socket, sign, sizeof(sign));
        if(rev <= 0)
        {
            perror("连接判断中断");
            return -1;
        }
        if(strstr(sign, "OK"))
        {
            printf("接到公告允许\n");
            break;
        }
    }
    //发送公告大小
    char eotice_size[4096] = {0};
    sprintf(eotice_size, "FILE_SIZE=[%d]", file_size);
    write(socket, eotice_size, strlen(eotice_size));

    while(1)
    {
        char sign[512] = {0};
        int rev = read(socket, sign, sizeof(sign));
        if(rev <= 0)
        {
            perror("连接判断中断");
            return -1;
        }
        if(strstr(sign, "SIZE_OK"))
        {
            printf("接到公告内容允许\n");
            break;
        }
    }

    return 1;
}


//公告服务函数
void notice_sever(int socket)
{
    //判断对方是否准备好接收，并发送文件的大小
                        //描述符  文件大小   标志位
    int sign = link_jodug(socket, strlen(NOTICE), "EOTICE"); 
    if(sign == -1)
    {
        printf("公告发送失败\n");
        return ;
    }
    
    write(socket, NOTICE, strlen(NOTICE));
    printf("发送公告完毕\n");

}


//列表服务函数
void list_sever(int socket, list_node *list_head)
{
    //获取好友列表的长度
    char list_msg[4096] = {0};

    list_node *pos, *n;
    int i=0;

    //获取好友列表并放到数组中
    list_for_each_safe_tail(list_head, pos, n)
    {
        int j=0;
        char buf[1024] = {0};
        char  *ip  = inet_ntoa(pos->msg.addr.sin_addr);  //获取对方地址
        sprintf(buf ,"ip :%s  socket :%d\n", ip, pos->msg.socket);
        while(buf[j] != '\0')
        {
            list_msg[i++] = buf[j++];
        }
    }
    

    printf("这是列表%s, 大小为[%ld]\n", list_msg, strlen(list_msg));

    int sign = link_jodug(socket, strlen(list_msg), "MAN_LIST"); 
    if(sign == -1)
    {
        printf("列表发送失败\n");
        return ;
    }
    
    write(socket, list_msg, strlen(list_msg));
    printf("发送列表完毕\n");

}


//接收文件

int rev_file(char *buf, list_node * my_node, list_node *you_node)
{
    //对方正在等待服务器的回应
    char filename[200] = {0};
    char p[54] = {0};
    int file_size;


    //******************************************************************
    sscanf(buf, "[FILE][%d]%s", &file_size,  p);
    printf("接收到文件名%s, 文件大小[%d]\n", p, file_size);

    /*

    聊天没有问题：
    聊天和文件的传输是放在一起的

    客户端选择了发送文件的时候会向服务器发送一个  FILE 标志位，判断后就能知道是信息还是文件
    rev_file

    传输文件有问题：目前发现截取名字有问题， 无法准确的获取到文件中的信息，
    特别是  截取字符串的时候，
    必须将截取到的名字打印出来查看是否有问题
    
    */


    //*******************************************************************


    sprintf(filename, "%s/%s", LOCAT, p);
    printf("要创建的文件名%s\n", filename);

    //回应对方已经准备好接收文件
    write(my_node->msg.socket,"OK",strlen("OK"));

    //创建文件
    int fd=open(filename,O_RDWR|O_CREAT|O_TRUNC,0777);
    if(fd < 0)
    {
        perror("创建失败");
        return 0;
    }
    //开始接收文件
    int  recv_size=0;
    while(1)
    {
        char   recv[4096]={0}; 
        int size = read(my_node->msg.socket,recv,4096); //读取网络数据 
        recv_size += size;
        printf("当前接收的大小 %d ,总大小 %d\n",size,recv_size);
        write(fd,recv,size); //写入到本地文件中  
        
        if(recv_size == file_size)
        {
            printf("接收完毕\n");
                //应答对方 
            write(my_node->msg.socket,"END",strlen("END"));
            close(fd); 
            break;
        }
    }
}


//聊天发送信息
int talk_other(list_node * my_node, list_node *you_node)
{
    my_node->msg.sta = IN_TALK;
    //问题：我现在给对方发送消息，消息必定要加一个标志位，对方要在收到标志位后判断是否是聊天信息，此时如果对方也在和服务器
    //通信就有可能导致线程之间的冲突，
    //解决办法是在用户信息的结构体中再加上一个状态标志，判断客户端单前的状态是否适合接收消息

 printf("2对方的ip是：%s\n", inet_ntoa(you_node->msg.addr.sin_addr));
    printf("对方已经开始聊天\n");

    //判断是否有留言
    if(my_node->msg.leave_sign == 1)
    {
        write(my_node->msg.socket, my_node->msg.leave, strlen(my_node->msg.leave));

        bzero(my_node->msg.leave, sizeof(my_node->msg.leave));  
        my_node->msg.leave_sign = 0;      
    }
    if(my_node->msg.file_sign == 1)
    {
        write(my_node->msg.socket, "由于你不在线，对方发送的文件过期了", strlen("由于你不在线，对方发送的文件过期了"));
        printf("结构体中存放的文件信息%s\n", my_node->msg.filename);
        my_node->msg.file_sign = 0;
    }

    int file = 0;//用于记录是收到了文件还是消息
    int msg = 0;

    while(1)
    {
        fd_set set;
        FD_ZERO(&set);

        int max = my_node->msg.socket;
        FD_SET(my_node->msg.socket, &set);

        int rev = select((max)+1, &set, NULL, NULL, NULL);
        if(rev < 0)
        {
            perror("select");
            return 0;
        }

        //开始监视
        if(FD_ISSET(my_node->msg.socket, &set))
        {
            char buf[4096] = {0};
            printf("my给you发送信息\n");
            
            while(1)
            {
                int rev_red = read(my_node->msg.socket, buf, 4096);
                if(rev_red <= 0)
                {
                    perror("对方断开连接\n");
                    return 0;
                } 

                if(strstr(buf, "EXIT"))
                {
                    printf("对方退出聊天总功能\n");
                    my_node->msg.sta = IN_FREE;
                    return 0;
                }

                printf("未处理的信息%s\n", buf);
                if(strstr(buf,"[TALK]"))
                {
                    char p[4000] = {0};
                    
                    msg = 1;
                    sscanf( buf, "[TALK]%s", p);
                    sprintf(buf, "[%s]%s", inet_ntoa(my_node->msg.addr.sin_addr), p);
                    printf("处理之后的信息%s\n", buf);

                    break;
                }
                if(strstr(buf, "[FILE]"))
                {
                    my_node->msg.sta = IN_FREE;

                    file = 1;
                    rev_file(buf, my_node, you_node);
                    //文件接收完毕，判断对方是否要接收数据
                    break;
                }

            }

            my_node->msg.sta = IN_TALK;
            //判断对方是否在聊天界面
            if(you_node->msg.sta != IN_TALK)
            {
                printf("you不在聊天界面\n");
                if(msg == 1)
                {
                    msg = 0;
                    //将留言最加到数组中
                    int i=0;
                    while(you_node->msg.leave[i++] != '\0');
                    strcpy(you_node->msg.leave+i, buf);
                    you_node->msg.leave_sign = 1;
                }
                if(file == 1)
                {
                    file = 0;

                    char p[512] = {0};
                    int file_size;
                    sscanf(buf, "[FILE][%d]%s", &file_size,  p);
                    //现在只能存储一个文件，后面新发的文件会覆盖旧的文件
                    //将别人发送的文件信息存放到结构体中
                    sprintf(you_node->msg.filename, "rm %s%s", LOCAT, p);
                    
                    system(you_node->msg.filename);

                    printf("节点中储存信息：%s\n", you_node->msg.filename);

                    you_node->msg.file_sign = 1;
                }
            }
            else if (you_node->msg.sta == IN_TALK)
            {
                printf("对方在聊天界面\n");
                //在聊天界面
                if(msg == 1)
                {
                    printf("直接给在聊天的人发送信息\n");
                    msg = 0;
                    printf("%s 给 %s 发信息\n", inet_ntoa(my_node->msg.addr.sin_addr), inet_ntoa(you_node->msg.addr.sin_addr));
                    write(you_node->msg.socket, buf, strlen(buf));
                }
                //让对方接收别人发送的文件
                if(file == 1)
                {                    
                    file = 0;
                    char name[1024] = {0};
                    char p[512] = {0};
                    int file_size;
                    sscanf(buf, "[FILE][%d]%s", &file_size, p);
                    sprintf( name, "[FILE]大小为[%d]文件[%s%s]",file_size, p, inet_ntoa(my_node->msg.addr.sin_addr));
                    write(you_node->msg.socket, name, strlen(name));
                    //发送文件

                    bzero(name, 1024);
                    sprintf(name, "%s/%s", LOCAT, p);

                    int fd = open(name,O_RDWR);
                    if(fd < 0)
                    {
                        perror("文件不存在");
                        break;
                    }


                    bzero(p, 512);
                    read(you_node->msg.socket, p, 512);
                    if(strstr(p, "OK"))
                    {
                        //开始发送
                        int  send_size=0;
                        while(1)
                        {
                            //读取文件中的数据  
                            char send[4096]={0};
                            int size=read(fd,send,4096);
                    
                            
                            printf("发送的大小 %d 总大小 %d\n",size,send_size);
                            
                            int  s_size=write(you_node->msg.socket,send,size);
                            send_size+=s_size;
                            if(send_size == file_size)
                            {
                                char head[64];
                                printf("发送完毕等待对方接收完毕\n");
                                bzero(head,64);
                                read(you_node->msg.socket,head,64);
                                if(strstr(head,"END"))
                                {
                                    char delfile[2000] = {0};
                                    sprintf(delfile, "rm %s", name);
                                    system(delfile);
                                    printf("文件传输完毕, 删除成功\n");
                                    break;
                                }
                            } 
                        }
                    }
                }
            }
        }    
    } 
}


//聊天判断信息
int talk_sever(list_node *msg_node)
{
    char other_ip[512] = {0};
    int sign = 0;

    list_sever(msg_node->msg.socket, msg_node->list_head);

    //标志位TALK_OTHER
    write(msg_node->msg.socket, "TALK_OTHER", strlen("TALK_OTHER"));

    printf("已发送聊天标志位\n");

    while(1)
    {
        list_node *pos, *n;

        read(msg_node->msg.socket, other_ip, sizeof(other_ip));
        printf("收到用户发来的ip[%s]\n", other_ip);
        
        //判断用户输入的ip是否真确
        list_for_each_safe_tail(msg_node->list_head, pos, n)
        {
            char ip[64] = {0};
            sprintf( ip,"%s", inet_ntoa(pos->msg.addr.sin_addr));
            printf("链表里的ip=%s\n", ip);
            //真确返回3个字节
            if(strcmp(inet_ntoa(pos->msg.addr.sin_addr), other_ip) == 0)
            {
                sign = 1;
                break;
            }
            else
            {
                continue;
            } 
        }
        if(sign == 1)
        {
            sign = 0;
            write(msg_node->msg.socket, "YYY", 3);
            printf("用户输入的是真确的ip\n");

            printf("1对方的ip是：%s\n", inet_ntoa(pos->msg.addr.sin_addr));
            //可以开始聊天
            talk_other(msg_node, pos);

            printf("聊天服务函数已退出\n");
            return 0;            
        }
        else
        {
            printf("用户输入的是错误的的ip\n");
            write(msg_node->msg.socket, "NNNN", 4);
            bzero(other_ip, 512);
        }
    }
}


//用户服务函数
void *user_sever(void *arg)
{
    //存放的的是用户的信息：ip地址和socket
    list_node *msg_node = (list_node *)arg;

    //给用户发送公告
    notice_sever(msg_node->msg.socket);


    printf("while上面\n");
    while(1)
    {
        fd_set set;
        FD_ZERO(&set);


        FD_SET(0, &set);
        FD_SET(msg_node->msg.socket, &set);

        //开始监视
        int rev = select((msg_node->msg.socket)+1, &set, NULL, NULL, NULL);
        if(rev < 0)
        {
            perror("select");
            return 0;
        }

        if(FD_ISSET(msg_node->msg.socket, &set))
        {
            char buf[1024] = {0};
            printf("有人发送信息\n");
            //如果接收信息出错则判断为下线
            int rev_red = read(msg_node->msg.socket, buf, 1024);
            if(rev_red <= 0)
            {
                printf("%s下线\n", inet_ntoa(msg_node->msg.addr.sin_addr));
                //用户人数减一
                people_num--;

                //删除这个用户节点
                remove_list_node(msg_node);
                free(msg_node);

                printf("删除结点成功\n");
                break;
            }

            printf("收到%s的信息[%s]\n",inet_ntoa(msg_node->msg.addr.sin_addr), buf);

            //用户需要好友列表
            if(strstr( buf,"LIST"))
            {
                msg_node->msg.sta = IN_LIST;
                list_sever(msg_node->msg.socket, msg_node->list_head);
                msg_node->msg.sta = IN_FREE;
            }

            //用户需要和人聊天
            if(strstr( buf, "TALK"))
            {
                talk_sever(msg_node);
                msg_node->msg.sta = IN_FREE;
            }
            
        }
    }
}


void *func(void *arg)
{
    list_node *list_head = (list_node *)arg;

    while(1)
    {
        display_list_node(list_head);
        printf("现有在线人数：%d\n", people_num);
        sleep(4);
    }

}




int main(int argc, char *argv[])
{
    if(argc != 3)
	{
		printf("./程序名  ip  port\n");
		return 0;
	}

    printf("请输入服务器存储位置(相对位置)：");
    scanf("%s", LOCAT);

    if(access(LOCAT, F_OK))
    {
        mkdir(LOCAT, 0777);
    }


    printf("请输入你要发布的公告:");
    scanf("%s", NOTICE);


    //创建终端连接
    int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(tcp_socket <= 0)
    {
        perror("");
        return 0;
    }
    else
    {
        printf("socket创建成功\n");
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;      //网络协议
    addr.sin_port = htons(atoi(argv[2]));    //端口号
    addr.sin_addr.s_addr = inet_addr(argv[1]);//ip地址

    //绑定
    int ret = bind(tcp_socket, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0)
    {
        perror("绑定失败\n");
        return 0;
    }
    else 
    {
        printf("绑定成功\n");
    }

    int rev_listen = listen(tcp_socket, 5);
    if(rev_listen < 0)
    {
        perror("监听失败\n");
        return 0;
    }
    else
    {
        printf("监听成功\n");
    }

    //初始化头结点
    list_node *list_head;
    struct sockaddr_in  exe;
    list_head = request_list_node(exe, -1, NULL);
    if(list_head == NULL)
    {
        printf("头结点初始化失败\n");
        return -1;
    }
    else
    {
        printf("头结点初始化成功\n");
    }

    pthread_t tid;
    //pthread_create(&tid, NULL, func, (void *)list_head);

    while(1)
    {
        
        //用于保存对方的地址信息
        struct sockaddr_in  clien_addr;
        int addr_len = sizeof(clien_addr);
        
        //4.接收客户端的链接请求 
        int  new_socket=accept(tcp_socket,(struct sockaddr *)&clien_addr,&addr_len);
        if(new_socket < 0)
        {
            perror("");
            break;
        }
        else
        {
            //转换IP地址 为字符串类型
            char  *ip  = inet_ntoa(clien_addr.sin_addr);  //获取对方地址
            printf("链接成功！！%d  IP: %s \n",new_socket,ip);
            
            //申请对方信息节点 
            list_node *new_node = request_list_node(clien_addr, new_socket, list_head);


            //在线用户加一
            people_num++; 

            new_node->msg.sta = IN_FREE;
            new_node->msg.leave_sign = 0;
            
            //插入节点
            insert_node_to_list_tail(list_head, new_node);
            //创建一个线程 
            pthread_t  pid;
            pthread_create(&pid,NULL,user_sever,(void *)new_node);
            //设置分离属性
            pthread_detach(pid);
        }
    }


    destroy_list(list_head);
    close(tcp_socket);
    return 0;
} 