#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct Chunk{
    uint64_t id=0;
    std::string text;
    std::string metadata;
    std::vector<float> embedding;
    void serialize(std::ostream &out) const;
    // 数据损坏时抛出 std::runtime_error
    static Chunk deserialize(std::istream &in);
};
