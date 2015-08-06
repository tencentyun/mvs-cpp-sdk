# uvs-cpp-sdk

#linux等类UINX系统使用手册
##需要安装的库和工具
openssl: ubuntu下运行 sudo apt-get install libssl-dev 安装  
其他类uinx系统按照各自的方法安装好该库  


curl: 在 Ubuntu 12.04.2 LTS,linux 版本 3.13.0-32-generic，编译了.a放在lib目录下。  
      如果有问题，在http://curl.haxx.se/download/curl-7.43.0.tar.gz 下载源码，  
      编译生成 .a 或者 .so 放到 lib目录下，替换掉原来的libcurl.a  

jsoncpp： 在 Ubuntu 12.04.2 LTS,linux 版本 3.13.0-32-generic，编译了.a放在lib目录下。  
      如果有问题，在 https://github.com/open-source-parsers/jsoncpp 下载源码，  
      编译生成 .a 或者 .so 放到 lib目录下，替换掉原来的libjsoncpp.a  

cmake: 去http://www.cmake.org/download/ 下载cmake安装好即可  

##编译生成静态库.a
执行下面的命令  
cd ${uvs-cpp-sdk}  
mkdir -p build  
cd build  
cmake ..  
make

需要将sample.cpp里的appid、secretId、secretKey、bucket等信息换成你自己的。  
生成的sample就可以直接运行，试用，  

生成的静态库，名称为:libuvsdk.a  

##将生成的库链接进自己的项目
生成的libuvsdk.a放到你自己的工程里lib路径下，  
include目录下的 Auth.h  Video.h curl  json  openssl都放到你自己的工程的include路径下。  

例如我的项目里只有一个sample.cpp,项目目录和sdk在同级目录，copy libuvsdk.a 到项目所在目录  
那么编译命令为:  
g++ -o sample sample.cpp -I ./include/ -L. -L../uvs-cpp-sdk/lib/ -luvsdk -lcurl -lcrypto -lssl -lrt -ljsoncpp

#windows系统咱不支持

#sample例子
使用接口前，必须调用：  
    Video::global_init();  
    Video api("your appid",
                "your secretId",
                "your secretKey",
                "interface timeout");

注意微视频上的path以 / 开头

##计算多次签名，静态函数任何地方可以直接调用
    string sign = Auth::appSign(
            10000379, "AKIDeQd0kEMr3bxf8QrBbRJwJNsm3wWYDxsu",
            "xsLWTDDbn8eHO1qfHeRlVhTMy5bzqePV",
            123, bucketName);
    cout << "sign:" << sign << endl;

##创建文件夹
    string strFoldername = "/test_cpp/";
	string strFilepath = "/test_cpp/test.mp4";
	string strFilepath_slice = "/test_cpp/test_slice.mp4";
	
    api.createFolder(
            bucketName, strFoldername);
    api.dump_res();
    
##列举文件夹下文件、目录
    api.listFolder(
            bucketName, "/", 10);
    api.dump_res();
    
##前缀查询
    api.prefixSearch(
            bucketName, "/test", 10);
    api.dump_res();
    
##更新目录
    api.updateFolder(
            bucketName, strFoldername, "0");
    api.dump_res();
    
##查询目录属性
    api.statFolder(
            bucketName, strFoldername);
    api.dump_res();
	
##直接上传
    api.upload(
            "../test.mp4", bucketName, 
            strFilepath);
    api.dump_res();

##大文件分片上传
    //sliceSize参数可以指定分片大小，默认是 512KB
    //后台允许的最大分片大小是3MB
    //如果中途失败，以相同的参数再次调用upload_slice可以自动断点续传
    api.upload_slice(
            "../test.log", bucketName, 
            strFilepath_slice);
	 
##文件更新
    api.update(
            bucketName, strFilepath, "attr","title","desc");
    api.dump_res();

##文件属性查询
    api.stat(
            bucketName, strFilepath);
    api.dump_res();

##目录删除
    api.delFolder(
            bucketName, strFoldername);
    api.dump_res();

##文件删除
    api.del(
            bucketName, strFilepath);
    api.dump_res();
