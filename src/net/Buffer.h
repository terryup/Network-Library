#ifndef BUFFER_H
#define BUFFER_H

#include "copyable.h"

#include <vector>
#include <string>
#include <algorithm>

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size

//  网络库底层的缓冲区定义
class Buffer : public copyable{
public:
    //  prependable 初始大小，readIndex 初始位置
    static const size_t kCheapPrepend = 8;
    //  writeable 初始大小，writeIndex 初始位置  
    //  刚开始 readerIndex 和 writerIndex 处于同一位置
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
        {}
    
    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const {
        return readerIndex_;
    }

    //  返回缓冲区中可读数据的起始地址
    const char* peek() const {
        return begin() + readerIndex_;
    }
    
    void retrieveUntil(const char *end){
        retrieve(end - peek());
    }

    //  onMessage string <- Buffer
    //  需要进行复位操作
    void retrieve(size_t len){
        //  应用只读取可读缓冲区数据的一部分(读取了len的长度)，还剩下readerIndex_ += len - writerIndex_
        if (len < readableBytes()){
            readerIndex_ += len;
        }
        //  全部读完 len == readableBytes()
        else{
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    //  吧onMessage上报的Buffer数据，转成string类型数据返回
    std::string retrieveAllAsString(){
        //  应用可读取数据的长度
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len){
        //  peek()可读数据的起始地址
        std::string result(peek(), len);
        //  上面一句把缓冲区中可读取的数据已经读取出来，所以要将缓冲区复位
        retrieve(len);
        return result;
    }

    //  buffer_.size() - writerIndex_   len
    void ensureWritableBytes(size_t len){
        if (writableBytes() < len){
            //  扩容函数
            makeSpace(len);
        }
    }

    void append(const std::string &str){
        append(str.data(), str.size());
    }

    //  把[data, data+len]内存上的数据添加到缓冲区中
    void append(const char *data, size_t len){
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite(){
        return begin() + writerIndex_;
    }

    const char* beginWriteConst() const {
        return begin() + writerIndex_;
    }
    
    //  从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    //  从fd上发送数据
    ssize_t writFd(int fd, int *saveErrno);

private:
    char* begin(){
        //  it.operator*().operator&()
        //  vector底层数组首元素的地址，也就是数组的起始地址
        return &(*buffer_.begin());
    }

    const char* begin() const {
        return &(*buffer_.begin());
    }

    void makeSpace(size_t len){
        /*
            kCheapPrepend | reader | writer |
            kCheapPrepend |       len         |
         */
        //  整个buffer都不够用
        if (writableBytes() + prependableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_ + len);
        }
        //  整个buffer够用，将后面移动到前面继续分配
        else{
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    //  采取 vector 形式，可以自动分配内存, 也可以提前预留空间大小
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;


};

#endif