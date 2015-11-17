#include <fstream>
#include "cstdio"
#include <cstring>
#include <unistd.h>
#include <errno.h>

#include <openssl/sha.h>

#include "Video.h"

namespace Qcloud_video{

const string Video::API_VIDEO_END_POINT = 
    "http://web.video.myqcloud.com/files/v1/";

size_t post_callback( 
        char *ptr, 
        size_t size, 
        size_t nmemb, 
        void *userdata) {

    Video * videoPtr = 
        static_cast<Video*>(userdata);
    size_t byte_num = size * nmemb;
    char tmp[byte_num+1];
    memcpy(tmp, ptr, byte_num);
    tmp[byte_num] = 0;
    videoPtr->response_str += tmp;
    return byte_num;
}

unsigned char ToHex(unsigned char x)
{
    static char table[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };
    return table[x];
}

//特殊的urlEncode，'/' 不转义
std::string videoUrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        if (isalnum((unsigned char)str[i]) ||
            (str[i] == '-') ||
            (str[i] == '_') ||
            (str[i] == '.') ||
            (str[i] == '~') ||(str[i] == '/'))

            strTemp += str[i];
        else
        {
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] % 16);
        }
    }
    return strTemp;
}

string genFileSHA1AndLen(
        const string &filePath,
        uint64_t *fileLen = NULL) {

    static char hexBytes[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8',
    '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    unsigned char sha1[SHA_DIGEST_LENGTH];
    ifstream fileInput(filePath.c_str(),
            ios::in | ios::binary);
    if (!fileInput) {
        return "";
    }

    if (fileLen != NULL) {
        fileInput.seekg(0 , std::ios::end);
        *fileLen = fileInput.tellg();
        fileInput.seekg(0 , std::ios::beg);
    }

    const int sliceSize = 2*1024*1024;
    char buf[sliceSize];
    SHA_CTX ctx;
    SHA1_Init(&ctx);

    uint64_t len = 0;
    while(!fileInput.eof()) {
        fileInput.read(buf, sliceSize);
        len = fileInput.gcount();
        SHA1_Update(
                &ctx, buf, len);
    }

    SHA1_Final(sha1, &ctx);

    fileInput.close();

    string shaStr = "";
    for (int i=0; i<SHA_DIGEST_LENGTH; i++) {
        shaStr += hexBytes[sha1[i] >> 4];
        shaStr += hexBytes[sha1[i] & 0x0f];
    }

    return shaStr;
}

const int Video::EXPIRED_SECONDS = 60;
const int Video::DEFAULT_SLICE_SIZE = 3145728;
const int Video::MIN_SLICE_FILE_SIZE = 10485760;
const int Video::MAX_RETRY_TIMES = 3;

const int Video::eMaskBizAttr = 1 << 0;
const int Video::eMaskTitle   = 1 << 1;
const int Video::eMaskDesc    = 1 << 2;
const int Video::eMaskVideoCover = 1 << 3;
const int Video::eMaskAll = Video::eMaskBizAttr | Video::eMaskTitle | Video::eMaskDesc | Video::eMaskVideoCover;

int32_t Video::global_init() {
    CURLcode retCode;
    retCode = curl_global_init(CURL_GLOBAL_ALL);
    if (CURLE_OK != retCode){
        return -1;
    }
    return 0;
}

void Video::global_finit() {
    curl_global_cleanup();
    return;
}

void Video::reset() {
    response_str = "";
    retCode = 0;
    retMsg = "";
    retJson.clear();
}

void Video::dump_res() {
    cout << "retJson:" << retJson << endl;
    cout << "retCode:" << retCode << endl;
    cout << "retMsg:" << retMsg << endl;
}

Video::Video(
        const uint64_t appid, 
        const string &secretId,
        const string &secretKey,
        uint64_t timeout)
            :APPID(appid),
            SECRET_ID(secretId),
            SECRET_KEY(secretKey) {

    _curl_handle = curl_easy_init();
    _timeout = timeout;
}

Video::~Video() {
    curl_easy_cleanup(_curl_handle);
}

string Video::generateResUrl(
        const string &bucketName,
        const string &dstPath) {

    string url = "";
    char urlBytes[10240];
    snprintf(urlBytes, sizeof(urlBytes),
            "%s%lu/%s%s",
            API_VIDEO_END_POINT.c_str(),
            APPID, 
            bucketName.c_str(), 
            dstPath.c_str());

    url = urlBytes;
    return url;
}

string Video::validFolderPath(const string &path) {
    if (path.empty()) {
        return "/";
    }

    string folderPath = path;
    if (folderPath[0] != '/') {
        folderPath = '/' + folderPath;
    }

    if (folderPath[folderPath.length() - 1] != '/') {
        folderPath = folderPath + '/';
    }

    return folderPath;
}

string Video::validFilePath(const string &path) {
    if (path.empty()) {
        return "/";
    }

    string filePath = path;
    if (filePath[0] != '/') {
        filePath = '/' + filePath;
    }

    return filePath;
}

int Video::sendRequest(
        const string &url,
        const int isPost,
        const vector<string> *headers,
        const char *data,
        struct curl_httppost * form_data) {

    curl_easy_reset(_curl_handle);

    CURLcode curl_ret = CURLE_OK;

    curl_easy_setopt(
            _curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(
            _curl_handle, CURLOPT_TIMEOUT, _timeout);

    if (isPost) {
        curl_easy_setopt(
            _curl_handle, CURLOPT_POST, 1);
        if (data) {
            curl_easy_setopt(
                _curl_handle, CURLOPT_POSTFIELDS, data);
        }

        if (form_data) {
            curl_easy_setopt(
                _curl_handle, CURLOPT_HTTPPOST, form_data);
        }
    }

    struct curl_slist *list = NULL;
    if (headers) {
        vector<string>::const_iterator it = 
            headers->begin();
        while (it != headers->end()) {
            list = curl_slist_append(
                    list, it->c_str());
            it++;
        }
        curl_easy_setopt(
                _curl_handle, CURLOPT_HTTPHEADER, list);

    }

    curl_easy_setopt(
            _curl_handle, CURLOPT_WRITEFUNCTION, 
            post_callback);
    curl_easy_setopt(
            _curl_handle, CURLOPT_WRITEDATA, 
            (void*)this );

    curl_ret = curl_easy_perform(_curl_handle);
    curl_slist_free_all(list);

    Json::Reader reader;
    if (reader.parse(response_str, retJson)) {
        retCode = retJson["code"].asInt();
        retMsg = retJson["message"].asString();
    } else {
        retCode = VIDEO_NETWORK_ERROR;
        retMsg = "net work err...";
    }
    
    return curl_ret;
}

int Video::upload(
        const string &srcPath,
        const string &bucketName,
        const string &dstPath,
        const string &videoCover,
        const string &bizAttr,
		const string &title,
		const string &desc,
		const string &magicContext) {

    reset();
    int ret = 0;
    ret = access(srcPath.c_str(), F_OK | R_OK);
    if (ret != 0) {
        retCode = VIDEO_FILE_NOT_EXISTS;
        retMsg = "file not exist or can not be read...";
        return retCode;
    }

    string encodePath = videoUrlEncode(dstPath);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath);

    string sign =
        Auth::appSign(
                APPID, SECRET_ID, SECRET_KEY,
                expired, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    string sha1 = genFileSHA1AndLen(srcPath);

    struct curl_httppost *firstitem = NULL,
                         *lastitem = NULL;

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "op",
            CURLFORM_COPYCONTENTS, "upload",
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "sha",
            CURLFORM_COPYCONTENTS, sha1.c_str(),
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "video_cover",
            CURLFORM_COPYCONTENTS, videoCover.c_str(),
            CURLFORM_END);
            
    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "biz_attr",
            CURLFORM_COPYCONTENTS, bizAttr.c_str(),
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "video_title",
            CURLFORM_COPYCONTENTS, title.c_str(),
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "video_desc",
            CURLFORM_COPYCONTENTS, desc.c_str(),
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "magicContext",
            CURLFORM_COPYCONTENTS, magicContext.c_str(),
            CURLFORM_END);
			
    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "filecontent",
            CURLFORM_FILE, srcPath.c_str(),
            CURLFORM_END);

    sendRequest(url, 1, &headers, NULL, firstitem);
    curl_formfree(firstitem);
    return retCode;
}

int Video::upload_slice(
        const string &srcPath,
        const string &bucketName,
        const string &dstPath,
        const string &videoCover,
        const string &bizAttr,
		const string &title,
		const string &desc,
		const string &magicContext,
        const int sliceSize,
        const string &session) {

    reset();
    int ret = 0;
    ret = access(srcPath.c_str(), F_OK | R_OK);
    if (ret != 0) {
        retCode = VIDEO_FILE_NOT_EXISTS;
        retMsg = "file not exist or can not be read...";
        return retCode;
    }

    string encodePath = videoUrlEncode(dstPath);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath);

    string sign =
        Auth::appSign(
                APPID, SECRET_ID, SECRET_KEY,
                expired, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    uint64_t fileSize = 0;
    string sha1 = genFileSHA1AndLen(srcPath, &fileSize);

    upload_prepare(fileSize, sha1, sign, url, videoCover, bizAttr, title, desc, magicContext, session, sliceSize);

    dump_res();

    if (retCode != 0) {
        return retCode;
    }

    if (retJson["data"].isMember("url")) {
        //秒传成功，直接返回了url
        return retCode;
    } 

    if (!retJson["data"].isMember("offset")
            || !retJson["data"].isMember("session")
            || !retJson["data"].isMember("slice_size")) {
        return retCode;
    }

    upload_data(fileSize, sha1, 
            retJson["data"]["slice_size"].asUInt64(),
            sign, url, srcPath, 
            retJson["data"]["offset"].asUInt64(),
            retJson["data"]["session"].asString());

    return retCode;
}

int Video::upload_prepare(
        const uint64_t fileSize,
        const string &sha,
        const string &sign,
        const string &url,
        const string &videoCover,
        const string &bizAttr,
		const string &title,
		const string &desc,
		const string &magicContext,
        const string &session,
        const uint64_t sliceSize
        ) {
    reset();

    int ret = 0;
    char buf[1024];
    struct curl_httppost *firstitem = NULL,
                         *lastitem = NULL;

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "op",
            CURLFORM_COPYCONTENTS, "upload_slice",
            CURLFORM_END);

    snprintf(buf, sizeof(buf), "%lu", fileSize);
    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "filesize",
            CURLFORM_COPYCONTENTS, buf,
            CURLFORM_END);

    ret = curl_formadd(&firstitem, &lastitem,
            CURLFORM_COPYNAME, "sha",
            CURLFORM_COPYCONTENTS, sha.c_str(),
            CURLFORM_END);

    if (!videoCover.empty()) {
        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "video_cover",
                CURLFORM_COPYCONTENTS, videoCover.c_str(),
                CURLFORM_END);
    }   
            
    if (!bizAttr.empty()) {
        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "biz_attr",
                CURLFORM_COPYCONTENTS, bizAttr.c_str(),
                CURLFORM_END);
    }

    if (!title.empty()) {
		ret = curl_formadd(&firstitem, &lastitem,
				CURLFORM_COPYNAME, "video_title",
				CURLFORM_COPYCONTENTS, title.c_str(),
				CURLFORM_END);
	}
	
	if (!desc.empty()) {
		ret = curl_formadd(&firstitem, &lastitem,
				CURLFORM_COPYNAME, "video_desc",
				CURLFORM_COPYCONTENTS, desc.c_str(),
				CURLFORM_END);
	}		
			
    if (!magicContext.empty()) {
		ret = curl_formadd(&firstitem, &lastitem,
				CURLFORM_COPYNAME, "magicContext",
				CURLFORM_COPYCONTENTS, magicContext.c_str(),
				CURLFORM_END);
	}
	
    if (!session.empty()) {
        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "session",
                CURLFORM_COPYCONTENTS, session.c_str(),
                CURLFORM_END);
    }

    if (sliceSize > 0) {
        if (sliceSize <= DEFAULT_SLICE_SIZE) {
            snprintf(
                buf, sizeof(buf), "%lu", sliceSize);
        } else {
            snprintf(
                buf, sizeof(buf), "%lu", sliceSize);
        }

        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "slice_size",
                CURLFORM_COPYCONTENTS, buf,
                CURLFORM_END);
    }

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    sendRequest(url, 1, &headers, NULL, firstitem);
    curl_formfree(firstitem);
    return retCode;
}

int Video::upload_data(
        const uint64_t fileSize,
        const string &sha,
        const uint64_t sliceSize,
        const string &sign,
        const string &url,
        const string &srcPath,
        const uint64_t offset,
        const string &session
        ) {

    int ret = 0;
    char tmp_buf[1024];
    char buf[sliceSize];

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    ifstream fileInput(srcPath.c_str(),
            ios::in | ios::binary);

    fileInput.seekg(offset);

    uint64_t pos = offset;
    while (fileSize > pos) {
        reset();
        uint64_t len =0;
        fileInput.read(buf, sliceSize);
        len = fileInput.gcount();

        struct curl_httppost *firstitem = NULL,
                             *lastitem = NULL;

        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "op",
                CURLFORM_COPYCONTENTS, "upload_slice",
                CURLFORM_END);

        snprintf(tmp_buf, sizeof(tmp_buf), 
                "%lu", pos);
        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "offset",
                CURLFORM_COPYCONTENTS, tmp_buf,
                CURLFORM_END);

        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "session",
                CURLFORM_COPYCONTENTS, session.c_str(),
                CURLFORM_END);

        ret = curl_formadd(&firstitem, &lastitem,
                CURLFORM_COPYNAME, "filecontent",
                CURLFORM_BUFFER, "data",
                CURLFORM_BUFFERPTR, buf,
                CURLFORM_BUFFERLENGTH, len,
                CURLFORM_END);

        int retry_times = 0;
        do {
            sendRequest(
                    url, 1, &headers, 
                    NULL, firstitem);
            dump_res();
            if (retCode == 0) {
                break;
            }
            retry_times++;
        } while(retry_times < MAX_RETRY_TIMES);

        curl_formfree(firstitem);

        if (retCode != 0) {
            return retCode;
        }

        pos += sliceSize;
    }

    return retCode;
}

int Video::createFolder(
        const string &bucketName, 
        const string &path,
        const string &biz_attr
        ) {
    reset();

    string folderPath = validFolderPath(path);
    string encodePath = videoUrlEncode(folderPath);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath); 

    string sign = 
        Auth::appSign(
                APPID, SECRET_ID, SECRET_KEY,
                expired, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);
    headers.push_back("Content-Type: application/json");

    Json::Value reqJson;
    reqJson["op"] = "create";
    if (!biz_attr.empty()) {
        reqJson["biz_attr"] = biz_attr;
    }
    Json::FastWriter writer;
    string data = writer.write(reqJson);
    //cout << "reqData:" << data << endl;

    sendRequest(url, 1, &headers, data.c_str());
    return retCode;
}

int Video::listFolder(
                    const string &bucketName, 
                    const string &path, 
                    const int num, 
                    const string &pattern,
                    const int order,
                    const string &context
        ) {
    string folderPath = validFolderPath(path);
    return listBase(bucketName, folderPath, num,
            pattern, order, context);
}

int Video::prefixSearch(
                    const string &bucketName, 
                    const string &prefix, 
                    const int num, 
                    const string &pattern,
                    const int order,
                    const string &context
        ) {
    string filePath = validFilePath(prefix);
    return listBase(bucketName, filePath, num,
            pattern, order, context);
}

int Video::listBase(
                    const string &bucketName, 
                    const string &path, 
                    const int num, 
                    const string &pattern,
                    const int order,
                    const string &context
        ) {
    reset();

    string encodePath = videoUrlEncode(path);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath); 
	string encodeContext = videoUrlEncode(context);

    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
            "?op=list&num=%d&pattern=%s&context=%s&order=%d", 
            num, pattern.c_str(), 
            encodeContext.c_str(), order);

    url += queryStr;

    //cout << "url:" << url << endl;

    string sign = 
        Auth::appSign(
                APPID, SECRET_ID, SECRET_KEY,
                expired, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    sendRequest(url, 0, &headers);

    return retCode;
}

int Video::updateFolder(
        const string &bucketName, 
        const string &path,
        const string &biz_attr
        ) {
    string folderPath = validFolderPath(path);
    return updateBase(bucketName, folderPath, biz_attr, "", "", "");
}

int Video::update(
        const string &bucketName, 
        const string &path,
        const string &videoCover,
        const string &biz_attr,
		const string &title,
		const string &desc
        ) {
    string filePath = validFilePath(path);
    return updateBase(bucketName, filePath, biz_attr, videoCover, title, desc);
}

int Video::updateBase(
        const string &bucketName, 
        const string &path,
        const string &biz_attr,
        const string &videoCover,
		const string &title,
		const string &desc
        ) {
    reset();

    if (path == "/") {
        retCode = VIDEO_PARAMS_ERROR;
        retMsg = "can not update bucket use api! go to http://console.qcloud.com/uvs/vbucket to operate bucket";
        return retCode;
    }

    string encodePath = videoUrlEncode(path);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath); 

    string sign = 
        Auth::appSign_once(
                APPID, SECRET_ID, SECRET_KEY,
                encodePath, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);
    headers.push_back("Content-Type: application/json");

    Json::Value reqJson;
    reqJson["op"] = "update";
    
    int flag = 0;
    if (!biz_attr.empty()) {
        flag |= eMaskBizAttr;
        reqJson["biz_attr"] = biz_attr;
    }    
	if (!title.empty()) {
        flag |= eMaskTitle;
        reqJson["video_tilte"] = title;
    }
	if (!desc.empty()) {
        flag |= eMaskDesc;
        reqJson["video_desc"] = desc;
    }
    if (!videoCover.empty()) {
        flag |= eMaskVideoCover;
        reqJson["video_cover"] = videoCover;
    }
    reqJson["flag"] = flag;
    Json::FastWriter writer;
    string data = writer.write(reqJson);
    //cout << "reqData:" << data << endl;

    sendRequest(url, 1, &headers, data.c_str());
    return retCode;
}

int Video::statFolder(
        const string &bucketName,
        const string &path
        ) {
    string folderPath = validFolderPath(path);
    return statBase(bucketName, folderPath);
}

int Video::stat(
        const string &bucketName,
        const string &path
        ) {
    string filePath = validFilePath(path);
    return statBase(bucketName, filePath);
}

int Video::statBase(
        const string &bucketName,
        const string &path
        ) {
    reset();

    string encodePath = videoUrlEncode(path);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath); 

    char queryStr[1024];
    snprintf(queryStr, sizeof(queryStr),
            "?op=stat");

    url += queryStr;

    //cout << "url:" << url << endl;

    string sign = 
        Auth::appSign(
                APPID, SECRET_ID, SECRET_KEY,
                expired, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);

    sendRequest(url, 0, &headers);

    return retCode;
}

int Video::delFolder(
        const string &bucketName,
        const string &path
        ) {
    string folderPath = validFolderPath(path);
    return delBase(bucketName, folderPath);
}

int Video::del(
        const string &bucketName,
        const string &path
        ) {
    string filePath = validFilePath(path);
    return delBase(bucketName, filePath);
}

int Video::delBase(
        const string &bucketName,
        const string &path
        ) {
    reset();

    if (path == "/") {
        retCode = VIDEO_PARAMS_ERROR;
        retMsg = "can not delete bucket use api! go to http://console.qcloud.com/uvs/vbucket to operate bucket";
        return retCode;
    }

    string encodePath = videoUrlEncode(path);
    uint64_t expired = time(NULL) + EXPIRED_SECONDS;
    string url = generateResUrl(bucketName, encodePath); 

    string sign = 
        Auth::appSign_once(
                APPID, SECRET_ID, SECRET_KEY,
                encodePath, bucketName);

    vector<string> headers;
    headers.push_back("Authorization: " + sign);
    headers.push_back("Content-Type: application/json");

    Json::Value reqJson;
    reqJson["op"] = "delete";
    Json::FastWriter writer;
    string data = writer.write(reqJson);
    //cout << "reqData:" << data << endl;

    sendRequest(url, 1, &headers, data.c_str());
    return retCode;
}



}//namespace Qcloud_video
