#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<stdio.h>

void encryptStr(char* p){
    char buf[16+1]={0};
    int i,flag;

    srand(time(NULL));
    for(i=0;i<8;i++){
        flag=rand()%3;
        switch(flag){
        case 0:
            buf[i]=rand()%26+'a';
            break;
        case 1:
            buf[i]=rand()%26+'A';
            break;
        case 2:
            buf[i]=rand()%10+'0';
            break;
        }
    }
    p[0]='$';
    p[1]='6';
    p[2]='$';
    p[3]=0;
    strcat(p,buf);
}
