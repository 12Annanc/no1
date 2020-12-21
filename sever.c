/*
    服务器

    可以再访问服务器的同时定义新的socket访问其他服务器

    功能：利用链表将用户信息存放起来，一个用户一个线程
          每次有用户连接上来就发送社区公告
          查询在线用户
          指定用户进行聊天
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


#define NOTICE "这是公告"


#define  PROT 7878
#define  IP "192.168.22.11"

int people_num = 0;


//安全的向上遍历（允许你在for循环当中任意操作pos节点而不影响链表的遍历）
#define list_for_each_safe_tail(head, pos, n)\
    for(pos=head->prev, n=pos->prev; pos != head; pos=n, n=n->prev)
    //安全的向下遍历（允许你在for循环当中任意操作pos节点而不影响链表的遍历）
#define list_for_each_safe(head, pos, n)\
    for(pos=head->next, n=pos->next; pos != head;  pos=n, n=n->next)


//存放用户信息
struct people
{
    int socket;  	
	struct sockaddr_in  addr;  
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



//公告服务函数
void notice_sever(int socket)
{
    char buf[512] = {0};
    //发送标志位
    write(socket, "EOTICE", strlen(NOTICE));
    
    while(1)
    {
        char sign[512] = {0};
        read(socket, sign, sizeof(sign));
        if(strstr(sign, "EOTICE_OK"))
        {
            printf("接到公告允许\n");
            break;
        }
    }
    //发送公告大小
    char eotice_size[4096] = {0};
    sprintf(eotice_size, "EOTICE_SIZE=[%ld]", sizeof(NOTICE));
    write(socket, eotice_size, strlen(eotice_size));

    while(1)
    {
        char sign[512] = {0};
        read(socket, sign, sizeof(sign));
        if(strstr(sign, "SIZE_OK"))
        {
            printf("接到公告内容允许\n");
            break;
        }
    }
    
    write(socket, NOTICE, strlen(NOTICE));

    printf("发送公告完毕\n");

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
        }
        
    }
}





int main(void)
{
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
    addr.sin_port = htons(PROT);    //端口号
    addr.sin_addr.s_addr = inet_addr(IP);//ip地址

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
    list_node *list_head;//链表头节点
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