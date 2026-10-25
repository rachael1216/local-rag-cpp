#include "config.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>

AppConfig::AppConfig(const std::string& filepath):config_path_(filepath){
    if(!loadFromFile(filepath)){
        std::println(std::cerr,"Warning: could not use config file '{}', falling back to built-in defaults (no model paths are set).",filepath);
    }
}

//读取 JSON 配置。先解析到局部副本，全部成功后才一次性生效，
//这样配置文件写坏了也不会留下"改了一半"的状态
bool AppConfig::loadFromFile(const std::string& filepath){
    std::ifstream file(filepath);
    if(!file.is_open()){
        return false;
    }
    modelConf models=models_;
    docConf doc=doc_;
    retrievalConf retrieval=retrieval_;
    generateModelConf gen_model=gen_model_;
    sampleConf sample=sample_;
    contextConf context=context_;
    try{
        nlohmann::json j;
        file>>j;
        if(j.contains("models")&&j["models"].is_object()){
            const auto& m=j["models"];
            models.emb_model_path=m.value("embedding", models.emb_model_path);
            models.gen_model_path=m.value("generation", models.gen_model_path);
            models.n_gpu_layers=m.value("n_gpu_layers", models.n_gpu_layers);
        }
        if(j.contains("document")&&j["document"].is_object()){
            const auto& d=j["document"];
            doc.chunk_size=d.value("chunk_size", doc.chunk_size);
            doc.overlap_size=d.value("overlap_size", doc.overlap_size);
        }
        if(j.contains("retrieval")&&j["retrieval"].is_object()){
            const auto& r=j["retrieval"];
            retrieval.top_k=r.value("top_k", retrieval.top_k);
        }
        if(j.contains("generation")&&j["generation"].is_object()){
            const auto& g=j["generation"];
            gen_model.gen_n_ctx=g.value("n_ctx", gen_model.gen_n_ctx);
            gen_model.gen_n_batch=g.value("n_batch", gen_model.gen_n_batch);
            gen_model.gen_n_ubatch=g.value("n_ubatch", gen_model.gen_n_ubatch);
            gen_model.max_output_tokens=g.value("max_output_tokens", gen_model.max_output_tokens);
            sample.temperature=g.value("temperature", sample.temperature);
            sample.top_k_sampler=g.value("top_k", sample.top_k_sampler);
            sample.top_p=g.value("top_p", sample.top_p);
        }
        if(j.contains("embedding")&&j["embedding"].is_object()){
            const auto& c=j["embedding"];
            context.emb_n_ctx=c.value("n_ctx", context.emb_n_ctx);
            context.emb_n_batch=c.value("n_batch", context.emb_n_batch);
            context.emb_n_ubatch=c.value("n_ubatch", context.emb_n_ubatch);
        }
    }
    catch(const std::exception& e){
        std::println(std::cerr,"Error parsing config file: {}", e.what());
        return false;
    }
    //把明显不合理的值拉回默认，避免后面出现除零、死循环或分配失败
    if(doc.chunk_size<=0) doc.chunk_size=500;
    if(doc.overlap_size<0) doc.overlap_size=0;
    if(doc.overlap_size>=doc.chunk_size) doc.overlap_size=doc.chunk_size/2;
    if(retrieval.top_k<=0) retrieval.top_k=3;
    if(gen_model.max_output_tokens<=0) gen_model.max_output_tokens=512;
    if(sample.temperature<0.0f) sample.temperature=0.0f;

    models_=models;
    doc_=doc;
    retrieval_=retrieval;
    gen_model_=gen_model;
    sample_=sample;
    context_=context;
    return true;
}

bool AppConfig::reload(){
    return loadFromFile(config_path_);
}
