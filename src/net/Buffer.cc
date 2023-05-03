#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
    从fd上读取数据  Poller工作在LT模式 
    Buffer缓冲区是有大小的！但是从fd上读数据的时候，却不知道TCP数据最终的大小
*/  
ssize_t Buffer::readFd(int fd, int *saveErrno){
    //  栈上的内存空间  暂时的缓冲区    64k
    char extrabuf[65536] = {0};
    //  使用iovec分配两个连续的缓冲区
    //  采用writev/readv来执行读写操作，所以定义一下两个成员
    struct iovec vec[2];
    //  获取Buffer底层缓冲区剩余可以读取的字节数(剩余可以读写的空间大小)
    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0){
        *saveErrno = errno;
    }
    //  Buffer的可写缓冲区已经够存储读出来的数据了
    else if(n <= writable){
        writerIndex_ += n;
    }
    //  extrabuf里面也写入了n-writable长度的数据
    else{
        writerIndex_ = buffer_.size();
        //  对buffer_扩容 并将extrabuf存储的另一部分数据追加至buffer_
        append(extrabuf, n - writable);
    }
    return n;
}

//  从fd上发送数据
//  inputBuffer_.readFd表示将对端数据读到inputBuffer_中，移动writerIndex_指针
//  outputBuffer_.writeFd表示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节
ssize_t Buffer::writFd(int fd, int *saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0){
        *saveErrno = errno;
    }
    return n;
}