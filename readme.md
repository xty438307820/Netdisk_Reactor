# 环境安装配置

安装mysql

```bash
sudo apt-get install mysql-server mysql-client
sudo apt-get install libmysqlclient-dev
```

以root身份进入mysql，这里root默认密码为空

```sql
sudo mysql -u root
//创建普通用户
mysql> CREATE USER 'abc'@'localhost' IDENTIFIED BY '123';
//创建数据库
mysql> create database FileServer;
//分配权限
mysql> GRANT ALL PRIVILEGES ON FileServer.* TO 'abc'@'localhost';
mysql> exit
//以普通用户登陆
mysql -uabc -p123
```

创建并初始化数据表

```sql
use FileServer;
create table UserInfo(
UserName varchar(128) primary key,
Salt char(32) not null,
PassWord char(128) not null
);
```

# 使用手册

## 一、基本功能

### 1.1 用户注册，密码验证

客户端进行用户密码验证后，才可进行操作，用户只能看到自己的文件，不能看到其他用户的文件。

服务端通过数据库存储用户名和密码。

### 1.2 用户操作

用户登录后，可以输入以下命令进行服务端上的文件查看：

```
1.cd [目录名]            #进入对应目录，cd .. 进入上级目录
2.ls                     #列出相应目录文件
3.puts [文件名]          #将本地文件上传至服务器
4.gets [文件名]          #下载服务器文件到本地
5.remove [文件、目录名]  #删除服务器上文件或目录
6.pwd                    #显示目前所在路径
7.mkdir [目录名]         #创建目录
8.clear                  #清空屏幕
9.其他命令不响应
```



# 遇到的问题

- (多线程信号处理问题)服务端往断开的客户端write时触发SIGPIPE信号导致服务端进程退出------solve:忽略SIGPIPE信号
