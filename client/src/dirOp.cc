#include <unistd.h>

//return 0表示文件存在
int fileExist(const char* path){
    return access(path,F_OK);
}