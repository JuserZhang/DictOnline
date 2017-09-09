/*************************************************************************
	> 文件名: server.c
	> 作者:张鹏 
	> 邮箱:1365385081@qq.com 
	> 创建时间: 2017年08月25日 星期五 21时30分29秒
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include<pthread.h>
   
#include<sqlite3.h>

#define IS_X64 1
#define DEBUG  0
#define SIZE   20

static char* user_id[10]={0};
static char* user_pwd[10]={0};
static int count=0;
static int user_no=1;  
sqlite3 *db=NULL;
char *errmsg=NULL;




//线程执行函数
void *thrd_func(void *arg);

//回调函数-> 查找用户个数
int find_user_no(void *prar,int f_num,char **f_value,char **f_name);

//回调函数-> 检索user表中的用户名
int callback(void *prar,int f_num,char **f_value,char **f_name);

//回调函数->查找单词
int look_for_word(void *prar,int f_num,char **f_value,char **f_name);

//把词典导入数据库表
void downlown_dict(void);

//字符串分离函数
void strbreak(char *word,char *mean,char *src);



int main(int argc ,char *argv[])
{
    //打开sqlite数据库
    if(0!=sqlite3_open("my.db",&db))
    {
        fprintf(stderr,"sqlite3_open:%s\n",sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }
    printf("................sqlite3_open success.................\n");
  
    //导入词典到数据库dict表,执行一次就行了
    // downlown_dict();

#if DEBUG  
    //打印获取的用户名跟密码
    int i;
    for(i=0;i<count;i++)
    {
        printf("id:%s\tpwd:%s\n",user_id[i],user_pwd[i]);
    }
#endif
    
    int listenfd;
    struct sockaddr_in servaddr,cliaddr;
    socklen_t  peerlen;

    if(argc!=3)
    {
        fprintf(stderr,"Usage: %s <ip> <port>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /*创建socket*/
    if(-1==(listenfd=socket(AF_INET,SOCK_STREAM,0)))
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //允许重用本机地址和端口
    int on=1;
    if(0>setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /*设置sockaddr_in 结构体参数*/
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(atoi(argv[2]));
    servaddr.sin_addr.s_addr=inet_addr(argv[1]);

    /*绑定函数bind*/
    if(bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr))<0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }


    /*调用listen()函数，设置监听模式*/
    if(-1==listen(listenfd,10))
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    /*调用accept()函数，等待客户端的连接*/
    pthread_t thread;
    pthread_attr_t attr;//线程属性
    int res;

    //初始化线程属性对象
    res=pthread_attr_init(&attr);
    if(res!=0)
    {
        printf("Create attribute failed\n");
        exit(res);
    }
    //设置线程分离属性
    if(0!=(res=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED)))
    {
        printf("Setting attribute failed\n");
        exit(res);
    }
#if IS_X64 
    long connfd;
#else 
    int connfd;
#endif    
    peerlen=sizeof(cliaddr);

    while(1)
    {    
        if(0>(connfd=accept(listenfd,(struct sockaddr *)&cliaddr,&peerlen)))
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("\nconnection from [%s:%d]\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));
        
        //创建线程
        res=pthread_create(&thread,&attr,thrd_func,(void*)connfd);
        if(0!=res)
        {
            printf("pthread_create failed\n");
            exit(res);//返回错误码
        }
    }

    sqlite3_close(db);
    //释放线程属性对象
    pthread_attr_destroy(&attr);      
    return 0;
}


/****************************线程执行函数************************/

void *thrd_func(void *arg)
{

#if IS_X64
    long connfd=(long)arg;
#else
    int connfd=(int)arg;
#endif

    char chose[20]={0};
    char id[20]={0};
    char pwd[20]={0};
    char word[25]={0};
    char buff[128]={0};
    char NON[128]="find failure!";
    char find_mean[256]={0};
    char SQL[128]={0};
    int ret;
    
    while(1)
    {
        //操作指令接收,决定是登录还是注册
        if(recv(connfd,chose,sizeof(chose),0)<0)
        {
            perror("recv");
            goto END;
        }
        //只有收到注册指令或登录指令才退出循环
        if(0==strcmp(chose,"1\n") || 0==strcmp(chose,"2\n" ))
        {
            ;
        }
        else
        {
            continue;
        }

        //发送空字符串,防止客服端阻塞
        if(send(connfd," ",1,0)<0)
        {
            ;
        }   


        //接收用户名
        ret = recv(connfd, id, sizeof(id), 0);
        if (ret < 0) {
            perror("recv");
            goto END;
        }
        //发送空字符串,防止客服端阻塞
        if(send(connfd," ",1,0)<0)
        {
            ;
        }
        //接收密码
        ret = recv(connfd, pwd, sizeof(pwd), 0);
        if (ret < 0) {
            perror("recv");
            goto END;
        }

        /*********************************************************
         *
         *                     注册操作
         *
         *********************************************************/
        if(0==strcmp(chose,"1\n"))
        {
            //操作sqlite数据库 -> 查找用户个数
            if(0!=sqlite3_exec(db,"select no from user;",find_user_no,NULL,&errmsg))
            {
                fprintf(stderr,"sqlite3_errmsg:%s\n",errmsg);
                exit(EXIT_FAILURE);
            }
            sprintf(SQL,"insert into user values(%d,'%s','%s');",user_no,id,pwd);
            //操作sqlite数据库->导入用户信息
            if(0!=sqlite3_exec(db,SQL,NULL,NULL,&errmsg))
            {
                fprintf(stderr,"sqlite3_errmsg:%s\n",errmsg);
                goto END;
            }    
        }

        /*********************************************************
         *
         *                     登录操作
         *
         *********************************************************/
        if(0==strcmp(chose,"2\n")) 
        {
            //操作sqlite数据库 -> 查找用户名,密码
            printf("###############所有用户信息###################\n");
            if(0!=sqlite3_exec(db,"select * from user;",callback,NULL,&errmsg))
            {
                fprintf(stderr,"sqlite3_errmsg:%s\n",errmsg);
                exit(EXIT_FAILURE);
            }
            printf("##############################################\n"); 
            //验证用户名跟密码
            int i;
            for(i=0;i<count;i++)
            {
                int ret_id,ret_pwd;
                ret_id=strcmp(id,user_id[i]);
                ret_pwd=strcmp(pwd,user_pwd[i]);
                if(0==ret_id && 0==ret_pwd)
                {
                    strcpy(buff,"logon success!");
                    if(send(connfd,buff,strlen(buff),0)<0)
                    {
                        ;
                    }
                    break;
                }
            }

            //遍历后发现数据库中没有与之匹配的用户,登录失败,结束线程
            if(i==count)
            {
                strcpy(buff,"logon failure!");
                if(send(connfd,buff,sizeof(buff),0)<0)
                {
                    ;
                }
                goto END;
            }
            printf("...............有用户上线.................\n");
            break;
        }
    }

    while (1) 
    {
        memset(word,0,sizeof(word));
        memset(find_mean,0,sizeof(find_mean));
        ret = recv(connfd, word, sizeof(word), 0);
        if (ret < 0) {
            perror("recv");
            break;
        }
        else if (ret == 0) 
        {
            printf("write close!\n");
            break;
        }
        
        //操作sqlite数据库 -> 查找单词
        sprintf(SQL,"select * from dict where word='%s';",word);
        if(0!=sqlite3_exec(db,SQL,look_for_word,(void*)find_mean,&errmsg))
        {
            fprintf(stderr,"sqlite3_errmsg:%s\n",errmsg);
            break;
        }    
        
        //没有找到结果
        if(strlen(find_mean)==0)
        { 
            if(send(connfd,NON,strlen(NON),0)<0)
            {
                perror("send");
                break;
            }
        }
        else
        {
            //返回查找的结果给客户端
            if(send(connfd,find_mean,strlen(find_mean),0)<0)
            {
                perror("send");
                break;
            }
       }
    }
    printf("................用户下线...................\n");
END:
    close(connfd);
    pthread_exit(NULL);
}


/******************回调函数->检索user表中的用户个数***********************/

int find_user_no(void *prar,int f_num,char **f_value,char **f_name)
{
    user_no++;
    return 0;
}

/******************回调函数->检索user表中所有的用户名***********************/

int callback(void *prar,int f_num,char **f_value,char **f_name)
{
    int i;
    for(i=0;i<f_num;i++)
    {
        printf("%-5s:%s\n",f_name[i],f_value[i]);
    }
    
    //存取获取的用户名ID
    user_id[count]=(char *)malloc(SIZE*sizeof(char));
    strcpy(user_id[count],f_value[1]);
    
    //存取获取的密码
    user_pwd[count]=(char*)malloc(SIZE*sizeof(char*));
    strcpy(user_pwd[count],f_value[2]);
    
    count++;
    return 0;
}

/****************************回调函数->查找单词**************************/

int look_for_word(void *prar,int f_num,char **f_value,char **f_name)
{
    int i;
    for(i=0;i<f_num;i++)
    {
        printf("%-5s:%s\n",f_name[i],f_value[i]);
    }
    strcpy((char *)prar,f_value[2]);
    return 0;
}

/******************************把字典写入数据库表***********************/

void downlown_dict(void)
{
    char dict_buff[512]={0};
    char dict_word[25]={0};
    char dict_mean[256]={0};
    char SQL[512]={0};
    FILE *fp;
    int i=1;
    size_t n=512;

#if DEBUG
    int max_mean=0;
    int max_word=0;
    char max_w[30]={0};
    char max_m[256]={0};
#endif

    fp=fopen("dict.txt","r");
    if(fp!=NULL)
    {
        while(fgets(dict_buff,sizeof(dict_buff),fp)!=NULL)
        {
            //分离单词跟注解
            strbreak(dict_word,dict_mean,dict_buff);
          
            //使用sprintf容易堆栈溢出,改用snprintf
            //sprintf(SQL,"insert into dict values(%d,'%s','%s');",i++,dict_word,dict_mean);
             snprintf(SQL,n,"insert into dict values(%d,'%s','%s');",i++,dict_word,dict_mean);
              
#if DEBUG 
            printf("dict_word:%ld\tdict_mean:%ld\n",strlen(dict_word),strlen(dict_mean));
            //求出最长的单词为23字符,最长的注释为255字符
            max_mean=max_mean>strlen(dict_mean)? max_mean:strlen(dict_mean);
            max_word=max_word>strlen(dict_word)? max_word:strlen(dict_word);
            if(strlen(dict_word)==max_word)
            strcpy(max_w,dict_word);
            if(strlen(dict_mean)==max_mean)
            strcpy(max_m,dict_mean);
#endif
            //操作sqlite数据库 -> 导入词典
            if(0!=sqlite3_exec(db,SQL,NULL,NULL,&errmsg))
            {
                fprintf(stderr,"sqlite3_errmsg:%s\n",errmsg);
                break;
            }    
            memset(dict_buff,0,sizeof(dict_buff));
            memset(dict_word,0,sizeof(dict_word));
            memset(dict_mean,0,sizeof(dict_mean));
            memset(SQL,0,sizeof(SQL));
        }

#if DEBUG
        printf("max_word=%d\tmax_mean=%d\n",max_word,max_mean);
        printf("%s..............%s\n",max_w,max_m);
#endif

    }
}

/***************************字符串分离函数****************************/

void strbreak(char *word,char *mean,char *src)
{
    //分离单词
    while(*src!=' ')
    {
        if(*src=='\'')
        {
            *src='`';//用`替换注解中的',方便后面导入表不发生语法错误;
        }
        *word=*src;
        word++;
        src++;
    }
    *word='\0';
    
    //跳过空格
    while(*src==' ')
    {
        src++;
    }

    //分离注解
    while(*src!='\n')
    { 
        if(*src=='\'')
        {
            *src='`';//用`替换注解中的',方便后面导入表不发生语法错误;
        }
        *mean=*src;
        mean++;
        src++;
    }
    *mean='\0';
}
