#pragma once
#include "chunk.h"
#include <cstddef>
#include <string>
#include <vector>

class VectorDatabase {
    public:
    VectorDatabase()=default;
    ~VectorDatabase()=default;
    void insert(const Chunk& chunk);
    void insert(const std::vector<Chunk>& chunks);
    // 返回余弦相似度最高的 top_k 个块，相似度从高到低排列
    std::vector<Chunk> search(const std::vector<float>& query_emb,size_t top_k) const;
    bool saveToDisk(const std::string& filename) const;
    bool loadFromDisk(const std::string& filename);
    size_t size() const{ return storage_.size();}
    // 库中向量的维度，库为空时返回 0
    size_t dimension() const;
    private:
    std::vector<Chunk> storage_;
    float cos_similarity(const std::vector<float>& a, const std::vector<float>& b) const;
};
