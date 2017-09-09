/*************************************************************************
    > 文件名: client.c
    > 作者:张鹏 
    > 邮箱:1365385081@qq.com 
    > 创建时间: 2017年08月22日 星期二 16时23分32秒
 ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>

static char user_id[20]={0};
static char user_pwd[20]={0};
static char buff[256]={0};
int write_num;


//注册
int  reg(int socket);

//登录验证用户名跟密码
int logon(int sockfd);

int main(int argc,char *argv[])
{
    if(argc<3)
    {
        printf("Usage: %s <srv_ip> <srv_port>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    /* 1.socket */
 
    int sockfd;
    sockfd=socket(AF_INET,SOCK_STREAM,0);
    if(0>sockfd)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* 2.connet */
    struct sockaddr_in srv_addr;
    memset(&srv_addr,0,sizeof(srv_addr));
    srv_addr.sin_family=AF_INET;
    srv_addr.sin_port=htons(atoi(argv[2]));
    srv_addr.sin_addr.s_addr=inet_addr(argv[1]);
    
    int ret;
    ret=connect(sockfd,(struct sockaddr *)&srv_addr,sizeof(srv_addr));
    if(0>ret)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    




    printf("\n............................连接服务器成功!.........................\n");   

    printf("\n............................Welcome...........................  ....\n");
    
    printf("............................ register: 1 ............................\n");
    printf("............................ logon:    2 ............................\n");
    printf("............................ quit:     3 ............................\n");
    
    while(1)
    { 
        char choose[20]={0};
        printf("请选择操作:");
        if(fgets(choose,sizeof(choose),stdin)==NULL)
        {

            perror("fgets");
            return 0;
        }
        //发送指令给服务器需要注册还是登录
        if(0==strcmp(choose,"1\n") || 0==strcmp(choose,"2\n"))
        {
            write_num=write(sockfd,choose,strlen(choose));
            if(0>write_num)
            {
                perror("write");
                return -1;
            }
            if(read(sockfd,buff,sizeof(buff))>0)
            {
                ;
            }
        }

        if(0==strcmp(choose,"1\n"))
        {
            //用户注册
            if(0!=reg(sockfd))
            {
                close(sockfd);
                return 0;
            }
            continue;
        }
        if(0==strcmp(choose,"2\n"))
        {
            //用户登录
            if(0!=logon(sockfd))
            {
                close(sockfd);
                return 0;
            }
            else
            {
                break;
            }
        }
        if(0==strcmp(choose,"3\n"))
        {
            close(sockfd);
            return 0;
        }
        else
        {
            continue;
        }
    }

    char path[50]={0};
    sprintf(path,".%s_histoy",user_id);//不同用户历史记录分开保存
    FILE *fp_w, *fp_r;
    char history[128]={0};
    if(NULL==(fp_w=fopen(path,"a+")))
    {
        perror("fopen");
        return 0;
    }
  
    /* 3.send/recv */
    while(1)
    {
        printf("\n请输入要查找的单词(exit: -1  history: ?):");
        memset(buff,0,sizeof(buff));
        if((fgets(buff,sizeof(buff),stdin))==NULL)
        {
            perror("fgets");
            break;
        }

         //保存查询记录,过滤掉?,-1.
        if(strcmp(buff,"?\n") && strcmp(buff,"-1\n") && strcmp(buff,"\n") && strlen(buff))
        {
            if(0>fwrite(buff,1,strlen(buff),fp_w))
            {
                perror("fwrite");
                break;
            }
        }     



        buff[strlen(buff)-1]='\0';//去掉回车字符
        if(strlen(buff)==0)//回车继续查询
        {
            continue;
        }

        //输入-1退出客户端
        if(0==strcmp(buff,"-1"))
        {
            fclose(fp_w);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        //查询历史记录
        if(0==strcmp(buff,"?"))
        {   
            int i=1;
            fclose(fp_w);//关闭写如历史记录文件流
            if(NULL==(fp_r=fopen(path,"r")))//打开读历史记录文件流
            {
                perror("fopen");
                return 0;
            }
            printf("\n_________________历史查询记录____________________\n");
            while(fgets(history,sizeof(history),fp_r))
            { 
                printf("%-2d: %s",i++,history);
            }
            printf("_________________________________________________\n");
            
            fclose(fp_r);//关闭读历史记录文件流
            if(NULL==(fp_w=fopen(path,"a+")))//重新打开写历史记录问价流
            {
                perror("fopen");
                return 0;
            }
            continue;
        }


        //把要查找的单词发送给服务器
        write_num=write(sockfd,buff,strlen(buff));
        if(0>write_num)
        {
            perror("write");
            break;
        }
       
        //接收查找到的结果
        memset(buff,0,sizeof(buff));
        if(read(sockfd,buff,sizeof(buff))>0)
        {
            printf("\n______________________________________________________________________________\n");
            printf("    %s\n",buff);
            printf("______________________________________________________________________________\n");
        }
        else
        {
            break;
        }
    } 
    /* 4.close */
    fclose(fp_w);
    close(sockfd);
    return 0;
}

/******************************用户注册***************************************/
int reg(int sockfd)
{
    printf("请输入一个用户名:");
    memset(user_id,0,sizeof(user_id));
    if(fgets(user_id,sizeof(user_id),stdin)==NULL)
    {
        perror("fgets");
        return -1;
    }


    printf("请输入一个密码:");
    memset(user_pwd,0,sizeof(user_pwd));
    if(fgets(user_pwd,sizeof(user_pwd),stdin)==NULL)
    {
        perror("fgets");
        return -1;
    }

    user_id[strlen(user_id)-1]='\0';
    user_pwd[strlen(user_pwd)-1]='\0';
    
    write_num=write(sockfd,user_id,strlen(user_id));
    if(0>write_num)
    {
        perror("write");
        return -1;
    }

    if(read(sockfd,buff,sizeof(buff))>0)
    {
        ;
    }

    write_num=write(sockfd,user_pwd,strlen(user_pwd));
    if(0>write_num)
    {
        perror("write");
        return -1;
    }

    return 0;
}

/***********************登录验证用户名跟密码**************************************/
int  logon(int sockfd)
{
    while(1)
    {
        printf("请输入用户名:");
        memset(user_id,0,sizeof(user_id));
        if(fgets(user_id,sizeof(user_id),stdin)==NULL)
        {
            perror("fgets");
            return -1;
        }


        printf("请输入密码:");
        memset(user_pwd,0,sizeof(user_pwd));
        if(fgets(user_pwd,sizeof(user_pwd),stdin)==NULL)
        {
            perror("fgets");
            return -1;
        }
        
        user_id[strlen(user_id)-1]='\0';
        user_pwd[strlen(user_pwd)-1]='\0';
      
        if(strlen(user_pwd)==0 || strlen(user_id)==0 ) 
        {
            printf("_____________________用户名或密码不能为空__________________________\n");
            continue;
        }
        break;
        
    }
    write_num=write(sockfd,user_id,strlen(user_id));
    if(0>write_num)
    {
        perror("write");
        return -1;
    }
    
    if(read(sockfd,buff,sizeof(buff))>0)
    {
        ;
    }

    write_num=write(sockfd,user_pwd,strlen(user_pwd));
    if(0>write_num)
    {
        perror("write");
        return -1;
    }

    memset(buff,0,sizeof(buff));
    if(read(sockfd,buff,sizeof(buff))>0)
    {
        printf("...........................%s......................................\n",buff);
        if(0==strcmp(buff,"logon failure!"))
        {
           close(sockfd);   //登录失败,关闭套接字,退出客户端
           return -1;
        }
    }
    return 0;
}

