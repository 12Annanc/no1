#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

pthread_t tid1, tid2;

void *func1(void *arg)
{
    char *ip = (char *)arg;
    char sign[1024] = {0};
    sprintf(sign,"ping %s > 1.txt", ip);
    printf("终端执行%s\n", sign);

    system(sign);
}

void *func2(void *arg)
{
    sleep(3);
    system("killall -s 9 ping");
    pthread_cancel(tid1);
}


//获取ip地址
char get_ip(char *ip)
{
    char msg[1024] = {0};
    FILE* fp;
    fp = fopen("./1.txt", "r");
    fgets(msg, 1024, fp);

    int i = 0;
    char *sign = strstr(msg ,"(");
    sign += 1;

    while((*sign) != ')')
    {
        ip[i++] = *sign;
        sign++;
    }

    fclose(fp);
}

void get_hostkey(char *host, char *key, char *adders)
{
    int i = 0;
    char *sign = strstr(adders,"//");
    sign += 2;
    printf("sign = %s\n", sign);

    while((*sign) != '/')
    {
        host[i++] = *sign;
        sign++;
    }

    strcpy(key, sign);
}


//https://file.alapi.cn/image/comic/214256-15045325768a2c.jpg


int main(int argc, char *argv[])
{

    char host[1024] = {0};
    char key[4096] = {0};

    get_hostkey(host, key, argv[1]);

    printf("host = %s\n", host);
    printf("key = %s\n", key);

    
    /*
    pthread_create(&tid1, NULL, func1, (void *)argv[1]);
    pthread_create(&tid2, NULL, func2, NULL);
    
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    char ip[1024] = {0};
    get_ip(ip);
    printf("ip = [%s]\n", ip);*/

    


    return 0;
}