#pragma once
#include "config.h"
#include "document.h"
#include "chunk.h"
#include "vectordatabase.h"
#include "embeddingengine.h"
#include "generationengine.h"
#include <llama.h>
#include <string>
#include <memory>
#include <vector>
struct BackendGuard {
    BackendGuard() { llama_backend_init(); }
    ~BackendGuard() { llama_backend_free(); }
    BackendGuard(const BackendGuard&)=delete;
    BackendGuard& operator=(const BackendGuard&)=delete;
};
class MragAPP{
    public:
    explicit MragAPP(const std::string& config_path);
    ~MragAPP()=default;
    // 重新读取配置文件。模型路径或上下文参数变了才会重新加载模型；失败时保留原来的引擎
    bool reload();
    const std::string& getConfigPath() const {return config_path_;}
    // 读取 txt、切分、嵌入并写入数据库文件。成功返回 true
    bool buildKnowledgeBase(const std::string& txt,const std::string& db);
    // 加载数据库并进入问答循环。数据库无法加载时返回 false
    bool chatloop(const std::string& db);
    private:
    // 成员的声明顺序决定析构顺序：引擎最先释放，llama 后端最后释放
    BackendGuard guard_;
    std::string config_path_;
    AppConfig config_;
    std::unique_ptr<EmbeddingEngine> embedding_engine_;
    std::unique_ptr<GenerationEngine> generation_engine_;
};
