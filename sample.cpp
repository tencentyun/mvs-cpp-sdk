#include "Video.h"
#include "cstring"

using namespace std;
using namespace Qcloud_video;

int main () {

    string bucketName = "testbucket";

    Video::global_init();
    Video api(10000379,
            "AKIDeQd0kEMr3bxf8QrBbRJwJNsm3wWYDxsu",
            "xsLWTDDbn8eHO1qfHeRlVhTMy5bzqePV", 30);
    
    //test sign
    /*
    string sign = Auth::appSign(
            10000379, "AKIDeQd0kEMr3bxf8QrBbRJwJNsm3wWYDxsu",
            "xsLWTDDbn8eHO1qfHeRlVhTMy5bzqePV",
            123, bucketName);
    cout << "sign:" << sign << endl;
    */

    //test createFolder
    string strFoldername = "/test_cpp/";
	string strFilepath = "/test_cpp/test.mp4";
	string strFilepath_slice = "/test_cpp/test_slice.mp4";
	
    api.createFolder(
            bucketName, strFoldername);
    api.dump_res();
    

    //test listFolder
    ///*
    api.listFolder(
            bucketName, "/", 10);
    api.dump_res();
    //*/

    //test prefixSearch
    
    api.prefixSearch(
            bucketName, "/test", 10);
    api.dump_res();
    

    //test updateFolder
    api.updateFolder(
            bucketName, strFoldername, "0");
    api.dump_res();
    
    //test statFolder
    api.statFolder(
            bucketName, strFoldername);
    api.dump_res();
	
	//test upload
    api.upload(
            "../test.mp4", bucketName, 
            strFilepath);
    api.dump_res();

    //test upload_slice
    api.upload_slice(
            "../test.log", bucketName, 
            strFilepath_slice);
	 
    //test update
    api.update(
            bucketName, strFilepath, "attr","title","desc");
    api.dump_res();

    //test stat
    api.stat(
            bucketName, strFilepath);
    api.dump_res();

    //test deleteFolder
    api.delFolder(
            bucketName, strFoldername);
    api.dump_res();

    //test delete
    api.del(
            bucketName, strFilepath);
    api.dump_res();

    api.del(
            bucketName, strFilepath_slice);
    api.dump_res();
	

    Video::global_finit();
    return 0;
}
