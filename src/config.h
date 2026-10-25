#pragma once
#include <string>
class AppConfig{
public:
//按用途把配置分成几个小结构体，通过下面的访问函数读取
struct modelConf{
    std::string emb_model_path="";
    std::string gen_model_path="";
    int n_gpu_layers=99;
    bool operator==(const modelConf&) const=default;
};
struct docConf{
    int chunk_size=500;
    int overlap_size=50;

};
struct retrievalConf{
    int top_k=3;
};
struct generateModelConf{
    int gen_n_ctx=4096;
    int gen_n_batch=4096;
    int gen_n_ubatch=512;
    int max_output_tokens=512;
};
struct sampleConf{
    float temperature=0.7;
    int top_k_sampler=40;
    float top_p=0.9;
};
struct contextConf{
    int emb_n_ctx=512;
    int emb_n_batch=512;
    int emb_n_ubatch=512;
    bool operator==(const contextConf&) const=default;
};
//构造和析构函数
explicit AppConfig(const std::string& filepath);
bool reload();
~AppConfig()=default;
//成员函数
const modelConf& model() const{return models_;}
const docConf& doc() const{return doc_;}
const retrievalConf& retrieval() const{return retrieval_;}
const generateModelConf& gen_model() const{return gen_model_;}
const sampleConf& sample() const{return sample_;}
const contextConf& context() const{return context_;}
private:
bool loadFromFile(const std::string& filepath);
std::string config_path_;
modelConf models_;
docConf doc_;
retrievalConf retrieval_;
generateModelConf gen_model_;
sampleConf sample_;
contextConf context_;

};
