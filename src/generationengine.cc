#include "generationengine.h"
#include <llama.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <print>
#include <sstream>
#include <stdexcept>

namespace {
// 保证任何退出路径（包括异常）都会释放 llama_batch
struct BatchGuard{
    llama_batch batch;
    explicit BatchGuard(llama_batch b):batch(b){}
    ~BatchGuard(){ llama_batch_free(batch); }
    BatchGuard(const BatchGuard&)=delete;
    BatchGuard& operator=(const BatchGuard&)=delete;
};
}

GenerationEngine::GenerationEngine(const std::string& model_path,const AppConfig& config)
:LlamaModelBase(model_path,ModelMode::Generation,config),config_(config){}

//拼接 Qwen 使用的 ChatML 提示词。参考片段和问题都放在同一个 user 回合里，
//最后以 assistant 标记结尾，让模型从这里开始续写
std::string GenerationEngine::makePrompt(const std::string& query,const std::vector<Chunk>& chunks) const{
    std::stringstream ss;
    ss<<"<|im_start|>system\n你是一个严谨的文档阅读助手。请你【必须并且只能】根据用户提供的【参考上下文】来回答问题。\n"
        "如果上下文中有答案，请详细回答并注明所依据的章节标签；如果上下文中没有提到相关信息，请直接回答“根据当前片段无法得出答案”。<|im_end|>\n";
    ss<<"<|im_start|>user\n参考上下文：\n";
    for(size_t i=0;i<chunks.size();++i){
        ss<<"[文章片段 "<<i+1<<" | 章节标签: "<<chunks[i].metadata<<"]: "<<chunks[i].text<<"\n";
    }
    ss<<"\n用户问题: "<<query<<"<|im_end|>\n";
    ss<<"<|im_start|>assistant\n";
    return ss.str();
}

//RAG 推理：预填充提示词，然后逐个采样并流式输出。返回完整回答
std::string GenerationEngine::generateStream(const std::string& query,const std::vector<Chunk>& chunks){
    const int n_ctx=static_cast<int>(llama_n_ctx(ctx_));
    //回答的长度上限不能超过上下文的一半，否则提示词没有位置放
    const int max_new=std::max(1,std::min(config_.gen_model().max_output_tokens,n_ctx/2));

    //提示词太长时，从相似度最低的片段开始丢（search 的结果是从高到低排序的），
    //这样被保留下来的永远是完整的提示词，问题和结尾标记不会被截掉
    std::vector<Chunk> used=chunks;
    std::vector<llama_token> tokens=tokenize(makePrompt(query,used),true);
    while(static_cast<int>(tokens.size())+max_new>n_ctx&&used.size()>1){
        used.pop_back();
        tokens=tokenize(makePrompt(query,used),true);
    }
    if(tokens.empty()){
        std::println(std::cerr,"Error: empty prompt");
        return "";
    }
    if(static_cast<int>(tokens.size())+max_new>n_ctx){
        std::println(std::cerr,"Error: the prompt needs {} tokens but the context is only {}. Lower document.chunk_size or raise generation.n_ctx.",tokens.size(),n_ctx);
        return "";
    }

    //清空 kv cache
    llama_memory_clear(llama_get_memory(ctx_),true);
    const struct llama_vocab* vocab=llama_model_get_vocab(model_);

    //预填充。提示词比 n_batch 长时分段提交，只有最后一个 token 需要输出 logits
    const size_t n_batch=std::max<size_t>(1,llama_n_batch(ctx_));
    for(size_t off=0;off<tokens.size();off+=n_batch){
        const size_t count=std::min(n_batch,tokens.size()-off);
        BatchGuard guard(llama_batch_init(static_cast<int32_t>(count),0,1));
        llama_batch& batch=guard.batch;
        batch.n_tokens=static_cast<int32_t>(count);
        for(size_t i=0;i<count;++i){
            batch.token[i]=tokens[off+i];
            batch.pos[i]=static_cast<llama_pos>(off+i);
            batch.n_seq_id[i]=1;
            batch.seq_id[i][0]=0;
            batch.logits[i]=(off+i==tokens.size()-1)?1:0;
        }
        int prefill_result=llama_decode(ctx_,batch);
        if(prefill_result!=0){
            throw std::runtime_error("Prefill llama_decode failed with code "+std::to_string(prefill_result));
        }
    }

    //采样链：top_k -> top_p -> temperature -> 按概率抽样。种子用随机值，
    //否则同一个问题每次都会得到一模一样的回答
    std::unique_ptr<llama_sampler,decltype(&llama_sampler_free)> sampler(
        llama_sampler_chain_init(llama_sampler_chain_default_params()),&llama_sampler_free);
    llama_sampler_chain_add(sampler.get(),llama_sampler_init_top_k(config_.sample().top_k_sampler));
    llama_sampler_chain_add(sampler.get(),llama_sampler_init_top_p(config_.sample().top_p,1));
    llama_sampler_chain_add(sampler.get(),llama_sampler_init_temp(config_.sample().temperature));
    llama_sampler_chain_add(sampler.get(),llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    //自回归生成：每一步只喂 1 个 token，复用同一个 batch
    BatchGuard step(llama_batch_init(1,0,1));
    std::string final_response;
    llama_pos n_past=static_cast<llama_pos>(tokens.size());
    for(int i=0;i<max_new;++i){
        llama_token new_token=llama_sampler_sample(sampler.get(),ctx_,-1);
        if(llama_vocab_is_eog(vocab,new_token)) break;

        std::vector<char> piece(64);
        int len=llama_token_to_piece(vocab,new_token,piece.data(),static_cast<int32_t>(piece.size()),0,true);
        if(len<0){
            piece.resize(static_cast<size_t>(-len));
            len=llama_token_to_piece(vocab,new_token,piece.data(),static_cast<int32_t>(piece.size()),0,true);
        }
        if(len>0){
            std::string out(piece.data(),static_cast<size_t>(len));
            final_response+=out;
            std::cout<<out<<std::flush;
        }

        llama_batch& batch=step.batch;
        batch.n_tokens=1;
        batch.token[0]=new_token;
        batch.pos[0]=n_past++;
        batch.n_seq_id[0]=1;
        batch.seq_id[0][0]=0;
        batch.logits[0]=1;
        if(llama_decode(ctx_,batch)!=0){
            std::println(std::cerr,"\nDecode error at step {}",i);
            break;
        }
    }
    std::cout<<std::endl;
    return final_response;
}
