#ifndef HTTPUTIL_H
#define HTTPUTIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include "filecipher.h"
#include "Traza.h"
#include "unzip/GZipHelper.h"
#include <curl/curl.h>
#include <gmutex.h>
#include "util/Progress.h"

#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL     3.0
#define MAX_FILE_BUFFER 5*1024*1024
#define MIN_PROGRESS_CHUNK_OF_BYTES 512
#define SECONDS_TO_ABORT_STUCK_DOWNLOAD 60

class HttpUtil
{
    public:
        HttpUtil();
        virtual ~HttpUtil();

        bool download(string);
        bool download(string, string);
        bool download(string, string, map <string, string> *headers);
        bool writeToFile(string);
        bool post(string, const char *,size_t, size_t, map <string, string> *);
        bool post(string, const char *,size_t, map <string, string> *);
        bool post(string, string, map <string, string> *);
        bool postDownload(string url, string data, map <string, string> *headers);
        bool get (string, map <string, string> *);
        bool put(string, const char*, size_t, size_t, map <string, string> *);
        
        bool del(string url, map <string, string> *headers);

        void setProxyIP(string var){proxyIP = var;}
        void setProxyPort(unsigned short var){proxyPort = var;}
        void setProxyUser(string var){proxyUser = var;}
        void setProxyPass(string var){proxyPass = var;}
        string getProxyIP(){return proxyIP;}
        unsigned short getProxyPort(){return proxyPort;}
        string getProxyUser(){return proxyUser;}
        string getProxyPass(){return proxyPass;}
        char * getRawData(){return chunk.memory;}

        char * getData(){
            CGZIP2A a((unsigned char *)chunk.memory, chunk.size);
            if(chunk.memory)
                free(chunk.memory);

            chunk.memory = (char *) malloc(strlen(a.psz));  /* will be grown as needed by the realloc above */
            chunk.size = strlen(a.psz);

            memcpy(chunk.memory, a.psz, strlen(a.psz));
            return chunk.memory;
        }

        size_t getDataLength(){return chunk.size;}
        int getTimeout(){return timeout;}
        void setTimeout(int var){timeout = var;}
        void abort(){aborted = true;}
        void setMaxBytesDownload(std::ifstream::pos_type var){prog->setMaxBytesDownload(var);}
        float getDownloadProgress(){return prog->getProgress();}
        double getDownloadSpeed(){return prog->getDlSpeed();}
        double getDownloadTotal(){return prog->getDlSizeBytes();}
        long getHttp_code(){return http_code;}
        map<string, string> *getResponseHeaders(){return &this->cabecerasResp;}
        void setSendContentLength(bool var){sendContentLength = var;}
        bool getSendContentLength(){return sendContentLength;}
        void setConnectionRetries(int connectionRetries);
        int getConnectionRetries() const;
        void setCookies(curl_slist* cookies);
        curl_slist* getCookies();
        
        
        
    protected:
    private:
        void init();
        string proxyIP;
        unsigned short proxyPort;
        string proxyUser;
        string proxyPass;
        
        struct MemoryStruct {
          char *memory;
          size_t size;
          char *filepath;
        };
        
        int connectionRetries;
        
        void parserCabeceras();
        map<string, string> cabecerasResp;

        struct MemoryStruct chunk;
        struct MemoryStruct header;
        int timeout;
        
        void cleanChunkData();
        Progress *prog;
        
        static std::string readBufferHeader;
        static size_t handleHeader(void *contents, size_t size, size_t nmemb, void *userp);
        static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
        static size_t read_callback(void *ptr, size_t size, size_t nmemb, FILE *stream);
        static void checkWriteMemToFile(MemoryStruct *mem);
        static int addDataToMem(void *contents, size_t realsize, MemoryStruct *mem);
        
        bool writeToFile(const char *, char *, ifstream::pos_type, bool);
        bool sendHttp(string url, const char* data, size_t, size_t, map <string, string> *headers, long httpType);
        bool sendHttpWithRetries(string url, const char* data, size_t tam, size_t offset, map <string, string> *headers, long httpType);
        
        static int older_progress(void *p,
                          double dltotal, double dlnow,
                          double ultotal, double ulnow);
        
        static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow);
        //This aborts all the running threads
        static bool aborted;
        long http_code;
        bool sendContentLength;
        GMutex mutex;
        struct curl_slist *cookies;
};

#endif // HTTPUTIL_H
