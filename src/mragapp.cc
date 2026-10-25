#include "mragapp.h"
#include <iostream>
#include <print>
#include <stdexcept>
#include "vectordatabase.h"

MragAPP::MragAPP(const std::string& config_path)
:config_path_(config_path),config_(config_path){
    const std::string emb_path=config_.model().emb_model_path;
    const std::string gen_path=config_.model().gen_model_path;
    if(!emb_path.empty()){
        embedding_engine_=std::make_unique<EmbeddingEngine>(emb_path, config_);
    }
    if(!gen_path.empty()){
        generation_engine_=std::make_unique<GenerationEngine>(gen_path, config_);
    }
}

bool MragAPP::reload(){
    //记下旧值，用来判断哪个模型真的需要重新加载
    const AppConfig::modelConf old_model=config_.model();
    const AppConfig::contextConf old_emb_ctx=config_.context();
    const AppConfig::generateModelConf old_gen=config_.gen_model();
    if(!config_.reload()){
        std::println(std::cerr,"Config reload failed, keeping the previous settings.");
        return false;
    }
    const AppConfig::modelConf& new_model=config_.model();
    const bool emb_changed=new_model.emb_model_path!=old_model.emb_model_path
        ||new_model.n_gpu_layers!=old_model.n_gpu_layers
        ||!(config_.context()==old_emb_ctx);
    const bool gen_changed=new_model.gen_model_path!=old_model.gen_model_path
        ||new_model.n_gpu_layers!=old_model.n_gpu_layers
        ||config_.gen_model().gen_n_ctx!=old_gen.gen_n_ctx
        ||config_.gen_model().gen_n_batch!=old_gen.gen_n_batch
        ||config_.gen_model().gen_n_ubatch!=old_gen.gen_n_ubatch;
    //先构造新引擎，成功后再替换，构造失败时旧引擎原样保留
    try{
        std::unique_ptr<EmbeddingEngine> new_emb;
        std::unique_ptr<GenerationEngine> new_gen;
        if(emb_changed&&!new_model.emb_model_path.empty()){
            new_emb=std::make_unique<EmbeddingEngine>(new_model.emb_model_path, config_);
        }
        if(gen_changed&&!new_model.gen_model_path.empty()){
            new_gen=std::make_unique<GenerationEngine>(new_model.gen_model_path, config_);
        }
        if(emb_changed) embedding_engine_=std::move(new_emb);
        if(gen_changed) generation_engine_=std::move(new_gen);
    }
    catch(const std::exception& e){
        std::println(std::cerr,"Config reloaded, but the models could not be reloaded: {}",e.what());
        return false;
    }
    std::println(std::cout,"Config reloaded successfully!");
    return true;
}

bool MragAPP::buildKnowledgeBase(const std::string& txt,const std::string& db) {
    if(!embedding_engine_){
        throw std::runtime_error("Embedding model is not configured: set models.embedding in config.json");
    }
    std::println(std::cout,"reading txt");
    DocumentProcessor processor(config_);
    std::vector<Chunk> chunks=processor.processNovel(txt);
    if(chunks.empty()){
        std::println(std::cerr,"No text could be read from {}",txt);
        return false;
    }
    std::println(std::cout,"chunk finished: {} chunks, embedding",chunks.size());
    for(size_t i=0;i<chunks.size();++i){
        chunks[i].embedding=embedding_engine_->generateEmbedding(chunks[i].text);
        if((i+1)%10==0||i+1==chunks.size()){
            std::println(std::cout,"embedded {}/{}",i+1,chunks.size());
        }
    }
    VectorDatabase vectordb;
    vectordb.insert(chunks);
    std::println(std::cout,"serializing");
    if(!vectordb.saveToDisk(db)){
        std::println(std::cerr,"serialize failed");
        return false;
    }
    std::println(std::cout,"serialize finished");
    return true;
}

bool MragAPP::chatloop(const std::string& db){
    if(!embedding_engine_){
        throw std::runtime_error("Embedding model is not configured: set models.embedding in config.json");
    }
    if(!generation_engine_){
        throw std::runtime_error("Generation model is not configured: set models.generation in config.json");
    }
    std::println(std::cout, "Loading database from:{}",db);
    VectorDatabase vectordb;
    if(!vectordb.loadFromDisk(db)){
        std::println(std::cerr,"load failed");
        return false;
    }
    std::println(std::cout,"Loaded {} chunks. Type a question, or exit to quit.",vectordb.size());
    std::string question;
    while(true){
        std::print(std::cout, "\n提问 >>> ");
        if(!std::getline(std::cin, question)){
            break;
        }
        if (question == "exit" || question == "quit") {
            break;
        }
        if(question=="/reload"){
            reload();
            continue;
        }
        if (question.empty()) {
            continue;
        }
        //单个问题出错（比如解码失败）不应该让整个会话退出
        try{
            std::vector<float> query_emb=embedding_engine_->generateEmbedding(question);
            if(vectordb.dimension()!=0&&query_emb.size()!=vectordb.dimension()){
                std::println(std::cerr,"The embedding model produces {}-dimensional vectors, but this database holds {}-dimensional ones. Rebuild it with --ingest.",query_emb.size(),vectordb.dimension());
                continue;
            }
            //每次提问都重新读取 top_k，这样 /reload 之后马上生效
            const size_t top_k=static_cast<size_t>(config_.retrieval().top_k);
            std::vector<Chunk> results=vectordb.search(query_emb,top_k);
            std::println(std::cout,"Retrieved {} relevant chunks.",results.size());
            generation_engine_->generateStream(question, results);
        }
        catch(const std::exception& e){
            std::println(std::cerr,"Error while answering: {}",e.what());
        }
        std::println(std::cout);
    }
    return true;
}
