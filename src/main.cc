#include "mragapp.h"
#include <cstring>
#include <iostream>
#include <print>
#include <string>
#include <llama.h>

//llama.cpp 默认会打印大量加载日志，这里只放行错误，其余丢弃
static void llama_log_callback(enum ggml_log_level level,const char* text,void* user_data){
    (void)user_data;
    if(level==GGML_LOG_LEVEL_ERROR){
        std::cerr<<text;
    }
}

static void printUsage(const char* prog){
    std::println(std::cerr,"Usage:");
    std::println(std::cerr,"  {} --ingest <text.txt> <database.db>   build a database from a text file",prog);
    std::println(std::cerr,"  {} --chat <database.db>                ask questions about it",prog);
    std::println(std::cerr,"Run it from the folder that holds config.json (the model paths in it are relative).");
}

int main(int argc,char* argv[]){
    llama_log_set(llama_log_callback, nullptr);
    if(argc<2){
        printUsage(argv[0]);
        return 1;
    }
    const std::string config_path="config.json";
    try{
        MragAPP app(config_path);
        if(std::strcmp(argv[1], "--ingest")==0){
            if(argc!=4){
                std::println(std::cerr,"--ingest needs two arguments: <text file> <database file>");
                return 1;
            }
            return app.buildKnowledgeBase(argv[2], argv[3])?0:1;
        }
        if(std::strcmp(argv[1], "--chat") == 0){
            if(argc!=3){
                std::println(std::cerr,"--chat needs one argument: <database file>");
                return 1;
            }
            return app.chatloop(argv[2])?0:1;
        }
        std::println(std::cerr,"Unknown option: {}",argv[1]);
        printUsage(argv[0]);
        return 1;
    }
    catch(const std::exception& e){
        std::println(std::cerr,"runtime error: {}",e.what());
        return 1;
    }
}
