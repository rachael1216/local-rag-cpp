#pragma once
#include "config.h"
#include <string>
#include <vector>
#include <llama.h>
struct llama_model;
struct llama_context;
enum class ModelMode{
    Embedding,
    Generation
};
class LlamaModelBase{
    public:
    LlamaModelBase(const std::string& model_path, ModelMode mode, const AppConfig& config);
    virtual ~LlamaModelBase();
    //禁用拷贝和移动：成员是裸指针，默认的移动只会复制指针，
    //移动后两个对象的析构函数会对同一个模型释放两次
    LlamaModelBase(const LlamaModelBase&)=delete;
    LlamaModelBase& operator=(const LlamaModelBase&)=delete;
    LlamaModelBase(LlamaModelBase&&)=delete;
    LlamaModelBase& operator=(LlamaModelBase&&)=delete;
    protected:
    std::vector<llama_token> tokenize(const std::string& text, bool add_special) const;
    struct llama_model* model_ = nullptr;
    struct llama_context* ctx_ = nullptr;
};