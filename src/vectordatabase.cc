#include "vectordatabase.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <print>
#include <queue>
#include <stdexcept>
#include <utility>

namespace {
constexpr uint32_t kMagic = 0x4D524147;  // "MRAG"
constexpr uint32_t kVersion = 1;
}

//单数据插入
void VectorDatabase::insert(const Chunk& chunk) {
    storage_.push_back(chunk);
}
//批量数据插入
void VectorDatabase::insert(const std::vector<Chunk>& chunks) {
    storage_.reserve(storage_.size() + chunks.size());
    storage_.insert(storage_.end(), chunks.begin(), chunks.end());
}
size_t VectorDatabase::dimension() const{
    for(const auto& chunk:storage_){
        if(!chunk.embedding.empty()) return chunk.embedding.size();
    }
    return 0;
}
//计算余弦相似度，维度不一致时返回 0
float VectorDatabase::cos_similarity(const std::vector<float>& a, const std::vector<float>& b) const{
    if(a.size()!=b.size()){
        return 0.0f;
    }
    float dotresult=0.0f;
    float norma=0.0f;
    float normb=0.0f;
    for(size_t i=0;i<a.size();i++){
        dotresult+=a[i]*b[i];
        norma+=a[i]*a[i];
        normb+=b[i]*b[i];
    }
    if(norma==0.0f||normb==0.0f){
        return 0.0f;
    }
    return dotresult/(std::sqrt(norma)*std::sqrt(normb));
}
//返回余弦相似度最高的 top_k 个块。用大小为 top_k 的最小堆，堆里只存相似度和下标，不复制整个 Chunk
std::vector<Chunk> VectorDatabase::search(const std::vector<float>& query_emb,size_t top_k)const{
    std::vector<Chunk> results;
    if(storage_.empty()||top_k==0){
        return results;
    }
    using Entry=std::pair<float,size_t>;  // (相似度, 下标)
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> minheap;
    for(size_t i=0;i<storage_.size();++i){
        const auto& chunk=storage_[i];
        if(chunk.embedding.empty()) continue;
        float similarity=cos_similarity(query_emb,chunk.embedding);
        if(minheap.size()<top_k){
            minheap.push({similarity,i});
        }
        else if(similarity>minheap.top().first){
            minheap.pop();
            minheap.push({similarity,i});
        }
    }
    results.resize(minheap.size());
    //堆顶是最小的，所以从后往前填，保证结果从高到低
    for(size_t k=minheap.size();k>0;--k){
        results[k-1]=storage_[minheap.top().second];
        minheap.pop();
    }
    return results;
}
//二进制序列化写盘，文件头是魔数和版本号
bool VectorDatabase::saveToDisk(const std::string& path) const{
    std::ofstream out(path,std::ios::out|std::ios::binary|std::ios::trunc);
    if(!out.is_open()){
        std::print(std::cerr,"Failed to open file:{}\n", path);
        return false;
    }
    uint32_t magic=kMagic;
    out.write(reinterpret_cast<const char*>(&magic),sizeof(magic));
    uint32_t version=kVersion;
    out.write(reinterpret_cast<const char*>(&version),sizeof(version));
    uint64_t chunk_count=static_cast<uint64_t>(storage_.size());
    out.write(reinterpret_cast<const char*>(&chunk_count),sizeof(chunk_count));
    for(const auto& chunk:storage_){
        chunk.serialize(out);
    }
    out.flush();
    if(!out){
        std::print(std::cerr,"Failed to write file:{}\n", path);
        return false;
    }
    return true;
}
//读取并校验文件头，失败时返回 false 并输出错误信息
bool VectorDatabase::loadFromDisk(const std::string& path){
    std::ifstream in(path,std::ios::in|std::ios::binary);
    if(!in.is_open()){
        std::print(std::cerr,"Failed to open file:{}\n", path);
        return false;
    }
    uint32_t magic=0;
    in.read(reinterpret_cast<char*>(&magic),sizeof(magic));
    if(!in || magic != kMagic){
        std::print(std::cerr,"Error: not a database file produced by this program: {}\n", path);
        return false;
    }
    uint32_t version=0;
    in.read(reinterpret_cast<char*>(&version),sizeof(version));
    if(!in || version!=kVersion){
        std::print(std::cerr,"Error: Unsupported database file version:{}\n", version);
        return false;
    }
    uint64_t chunk_count=0;
    in.read(reinterpret_cast<char*>(&chunk_count),sizeof(chunk_count));
    if(!in){
        std::print(std::cerr,"Error: database header is truncated\n");
        return false;
    }
    std::vector<Chunk> loaded;
    //块数来自文件内容，先用一个上限保护 reserve，真正的数量由后面的逐块读取决定
    loaded.reserve(static_cast<size_t>(std::min<uint64_t>(chunk_count, 1u<<20)));
    try{
        for(uint64_t i=0;i<chunk_count;++i){
            loaded.push_back(Chunk::deserialize(in));
        }
    }
    catch(const std::exception& e){
        std::print(std::cerr,"Error: database is corrupted ({})\n", e.what());
        return false;
    }
    storage_=std::move(loaded);
    return true;
}
