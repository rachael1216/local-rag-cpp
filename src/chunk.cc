#include "chunk.h"
#include <istream>
#include <ostream>
#include <stdexcept>

namespace {
// 读取时对长度字段做上限检查，防止文件损坏时一次分配几十 GB 内存
constexpr uint64_t kMaxStringBytes = 1ull << 28;   // 256 MiB
constexpr uint64_t kMaxEmbeddingDim = 1ull << 20;  // 约一百万维，远超任何实际模型
}

//二进制序列化。字段顺序：id，text 长度+内容，metadata 长度+内容，embedding 维度+浮点数组（长度统一为 uint64）
void Chunk::serialize(std::ostream &out) const{
    out.write(reinterpret_cast<const char*>(&id), sizeof(id));
    uint64_t text_len=static_cast<uint64_t>(text.size());
    out.write(reinterpret_cast<const char*>(&text_len), sizeof(text_len));
    if(text_len>0) out.write(text.data(), static_cast<std::streamsize>(text_len));
    uint64_t metadata_len=static_cast<uint64_t>(metadata.size());
    out.write(reinterpret_cast<const char*>(&metadata_len), sizeof(metadata_len));
    if(metadata_len>0) out.write(metadata.data(), static_cast<std::streamsize>(metadata_len));
    uint64_t embedding_dim=static_cast<uint64_t>(embedding.size());
    out.write(reinterpret_cast<const char*>(&embedding_dim), sizeof(embedding_dim));
    if(embedding_dim>0) out.write(reinterpret_cast<const char*>(embedding.data()), static_cast<std::streamsize>(embedding_dim*sizeof(float)));
}

Chunk Chunk::deserialize(std::istream &in){
    Chunk chunk;
    in.read(reinterpret_cast<char*>(&chunk.id), sizeof(chunk.id));
    uint64_t text_len=0;
    in.read(reinterpret_cast<char*>(&text_len),sizeof(text_len));
    if(!in || text_len>kMaxStringBytes) throw std::runtime_error("Deserialization failed: bad text length");
    if(text_len>0){
        chunk.text.resize(text_len);
        in.read(&chunk.text[0], static_cast<std::streamsize>(text_len));
    }
    uint64_t metadata_len=0;
    in.read(reinterpret_cast<char*>(&metadata_len),sizeof(metadata_len));
    if(!in || metadata_len>kMaxStringBytes) throw std::runtime_error("Deserialization failed: bad metadata length");
    if(metadata_len>0){
        chunk.metadata.resize(metadata_len);
        in.read(&chunk.metadata[0], static_cast<std::streamsize>(metadata_len));
    }
    uint64_t embedding_dim=0;
    in.read(reinterpret_cast<char*>(&embedding_dim),sizeof(embedding_dim));
    if(!in || embedding_dim>kMaxEmbeddingDim) throw std::runtime_error("Deserialization failed: bad embedding size");
    if(embedding_dim>0){
        chunk.embedding.resize(embedding_dim);
        in.read(reinterpret_cast<char*>(chunk.embedding.data()), static_cast<std::streamsize>(embedding_dim*sizeof(float)));
    }
    if(!in) throw std::runtime_error("Deserialization failed: unexpected end of file");
    return chunk;
}
