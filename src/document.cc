#include "document.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <print>
#include <regex>

namespace {
inline bool isContinuation(unsigned char c){ return (c&0xC0)==0x80; }

// 判断 text 中 [pos, pos+len) 这个字符是不是适合作为切分点的标点
// 英文标点：换行、. ! ?；中文标点：， 。 ！ ？（均为 3 字节 UTF-8）
bool isSplitChar(const std::string& text, size_t pos, size_t len){
    if(len==1){
        char c=text[pos];
        return c=='\n'||c=='.'||c=='!'||c=='?';
    }
    if(len==3){
        static const char* const kPunct[]={"\xEF\xBC\x8C","\xE3\x80\x82","\xEF\xBC\x81","\xEF\xBC\x9F"}; // ， 。 ！ ？
        for(const char* p:kPunct){
            if(std::memcmp(text.data()+pos,p,3)==0) return true;
        }
    }
    return false;
}
}

DocumentProcessor::DocumentProcessor(const AppConfig& config):config_(config){}

//如果 index 落在一个多字节字符中间，就向后移动到下一个字符的开头
size_t DocumentProcessor::nextUtf8Boundary(const std::string& text, size_t index) const{
    if(index>=text.size()) return text.size();
    while(index<text.size()&&isContinuation(static_cast<unsigned char>(text[index]))){
        index++;
    }
    return index;
}

//在 [start, end) 窗口里，从 end 往回最多找 100 个字符，找到标点就在它后面切；
//找不到就在 end 处切。返回值一定落在字符边界上，且大于 start。
size_t DocumentProcessor::findSplitPoint(const std::string& text, size_t start, size_t end) const {
    if(end>=text.size()) return text.size();
    //先把 end 对齐到字符开头，保证 [start,end) 不会截断一个汉字
    while(end>start&&isContinuation(static_cast<unsigned char>(text[end]))) --end;
    if(end==start){
        //窗口比一个字符还小（chunk_size 设得极小才会发生），至少前进一个完整字符
        end=nextUtf8Boundary(text,start+1);
        return end;
    }
    size_t i=end;
    for(int scanned=0;i>start&&scanned<100;++scanned){
        size_t j=i-1;
        while(j>start&&isContinuation(static_cast<unsigned char>(text[j]))) --j;
        if(isSplitChar(text,j,i-j)) return i;
        i=j;
    }
    return end;
}

//读取文本、识别章节标题、用滑动窗口切分
std::vector<Chunk> DocumentProcessor::processNovel(const std::string& filepath) const{
    std::vector<Chunk> chunks;
    std::ifstream file(filepath,std::ios::in|std::ios::binary);
    if(!file.is_open()){
        std::print(std::cerr,"Failed to open file: {}\n", filepath);
        return chunks;
    }
    std::string full_text;
    std::string line;
    std::vector<std::pair<size_t, std::string>> chapters;
    //章节标题：第一章 / 第十二回 / 第3节 / Chapter 7。数字后必须紧跟 章节回卷 之一，
    //这样正文里以"第"开头的句子（"第二天清晨，……回到……"）不会被当成标题
    static const std::regex chapter_regex(
        R"(^\s*(?:第\s*(?:[0-9]+|(?:零|〇|一|二|两|三|四|五|六|七|八|九|十|百|千)+)\s*(?:章|节|回|卷)(?:\s|$|:|：|、|\.|．)|Chapter\s+[0-9]+))");
    bool first_line=true;
    while(std::getline(file,line)){
        if(first_line){
            first_line=false;
            //去掉 UTF-8 BOM，否则它会粘在第一行开头
            if(line.size()>=3&&std::memcmp(line.data(),"\xEF\xBB\xBF",3)==0) line.erase(0,3);
        }
        if(!line.empty()&&line.back()=='\r'){
            line.pop_back();
        }
        if(std::regex_search(line, chapter_regex)){
            chapters.push_back({full_text.size(), line});
        }
        full_text+=line;
        full_text+='\n';
    }
    file.close();

    size_t chunk_size=static_cast<size_t>(config_.doc().chunk_size);
    size_t overlap_size=static_cast<size_t>(config_.doc().overlap_size);
    if(chunk_size==0) chunk_size=500;
    if(overlap_size>=chunk_size) overlap_size=chunk_size/2;

    //滑动窗口：每块最长 chunk_size 字节，下一块从上一块结尾往回 overlap_size 字节处开始
    size_t start=0;
    uint64_t chunkID=0;
    size_t chapter_idx=0;
    std::string current_chapter;
    while(start<full_text.size()){
        //章节按起点判断：起点已经越过某个标题，就归入那一章
        while(chapter_idx<chapters.size()&&chapters[chapter_idx].first<=start){
            current_chapter=chapters[chapter_idx].second;
            ++chapter_idx;
        }
        size_t end=start+chunk_size;
        if(end<full_text.size()){
            end=findSplitPoint(full_text,start,end);
        }
        else{
            end=full_text.size();
        }
        std::string piece=full_text.substr(start,end-start);
        //只含空白的块没有信息量，嵌入后还会污染检索结果，直接跳过
        if(piece.find_first_not_of(" \t\r\n")!=std::string::npos){
            Chunk chunk;
            chunk.id=chunkID++;
            chunk.text=std::move(piece);
            chunk.metadata=current_chapter.empty()?"Before Text":current_chapter;
            chunks.push_back(std::move(chunk));
        }
        if(end>=full_text.size()) break;
        size_t next_start=end>overlap_size?end-overlap_size:end;
        size_t next=nextUtf8Boundary(full_text,next_start);
        //必须向前推进，否则会死循环；重叠区放不下时直接从 end 开始
        if(next<=start||next>end) next=end;
        start=next;
    }
    return chunks;
}
