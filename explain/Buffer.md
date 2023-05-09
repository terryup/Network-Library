# Buffer

## Buffer是什么，为什么要有Buffer？

`Buffer`称为缓冲区，发送方将数据写入缓冲区，而接收方从缓冲区中读取数据。Buffer主要用于接收和发送数据，可以有效地提高数据的传输效率。例如，在读取`socket`上的数据时，先将数据读入到`Buffer`中，然后再对数据进行解析，这种方式能够有效地减少系统调用的次数，提高IO效率。

对于发送缓冲区，当应用程序需要发送数据时，先将数据写入缓冲区，然后由内核负责将缓冲区中的数据发送出去，直到发送完成才返回。如果没有缓冲区，应用程序需要等待直到内核将数据发送出去，而这段时间是比较长的，会降低程序的运行效率。

对于接收缓冲区，当内核收到数据时，会先将数据暂存在缓冲区中，等待应用程序来读取。如果没有缓冲区，内核就需要将数据一字节一字节地传递给应用程序，这样会增加内核的负担，也会降低程序的运行效率。

### Buffer的数据结构

![](/Users/zixuanhuang/Desktop/webserver_explain/20200518112726386.png)

```c++
//  采取 vector 形式，可以自动分配内存, 也可以提前预留空间大小
std::vector<char> buffer_;
size_t readerIndex_;
size_t writerIndex_;
```

buffer的内部是一个`vector<char>`，它是一块连续的内存。他有两个成员指向了`vector<char>`，分别是`readerIndex_`和`writerIndex_`。

- prependable=readIndex
- readable=writeIndex-readIndex
- writable=size()-writeIndex

当初始化后，`readerIndex_`和`writerIndex_`会指向同一个位置。

![](/Users/zixuanhuang/Desktop/webserver_explain/20200518114416521.png)

```c++
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
private:
      //  采取 vector 形式，可以自动分配内存, 也可以提前预留空间大小
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
```

### 读写数据时对Buffer的操作

向Buffer写入200字节：

![](/Users/zixuanhuang/Desktop/webserver_explain/20200518115637104.png)

从Buffer读入50字节：

![](/Users/zixuanhuang/Desktop/webserver_explain/20200518120034738.png)

从Buffer一次性读入150字节：

![](/Users/zixuanhuang/Desktop/webserver_explain/20200518120440227.png)

```c++
    void ensureWritableBytes(size_t len){
        if (writableBytes() < len){
            //  扩容函数
            makeSpace(len);
        }
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
```

### readFd函数

`readFd`函数。该函数会从fd上读取数据，并将读取到的数据存储到Buffer类对象的底层缓冲区中，具体实现方式如下：

1. 先定义一个栈上的内存空间 `extrabuf`，作为一个暂时的缓冲区，大小为 64k。
2. 使用`struct iovec`结构体定义两个连续的缓冲区，采用`readv`系统调用来执行读操作。
3. 确定使用的缓冲区个数（最多两个），根据剩余的可写缓冲区大小来决定是否需要使用`extrabuf`。
4. 执行`readv`操作，将读取到的数据存储到上述定义的缓冲区中。
5. 根据返回值n的不同，修改Buffer对象的`writerIndex_`指针。

```c++
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
```

### writFd函数

`writeFd`函数是从`Buffer`对象的底层缓冲区中读取数据，并将其写入到fd上，具体实现方式如下：

1. 首先使用`peek()`函数获得底层缓冲区中的可读数据。
2. 执行`write`操作，将数据写入fd中。
3. 根据返回值n的不同，修改Buffer对象的`readerIndex_`指针。

```c++
ssize_t Buffer::writFd(int fd, int *saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0){
        *saveErrno = errno;
    }
    return n;
}
```

## 参考

[Muduo库中的Buffer设计](https://blog.csdn.net/qq_36417014/article/details/106190964#:~:text=Muduo%E5%BA%93%E4%B8%AD%E7%9A%84Buffer%E7%B1%BB%E8%AE%BE%E8%AE%A1%20%E9%9D%9E%E9%98%BB%E5%A1%9E%E7%BD%91%E7%BB%9C%E7%BC%96%E7%A8%8B%E4%B8%AD%E5%BA%94%E7%94%A8%E5%B1%82buffer%E6%98%AF%E5%BF%85%E9%A1%BB%E7%9A%84%20%E5%8E%9F%E5%9B%A0%EF%BC%9A%E9%9D%9E%E9%98%BB%E5%A1%9EIO%E7%9A%84%E6%A0%B8%E5%BF%83%E6%80%9D%E6%83%B3%E6%98%AF%E9%81%BF%E5%85%8D%E9%98%BB%E5%A1%9E%E5%9C%A8read%20%28%29%E6%88%96write%20%28%29%E6%88%96%E5%85%B6%E4%BB%96IO%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8%E4%B8%8A%EF%BC%8C%E8%BF%99%E6%A0%B7%E5%8F%AF%E4%BB%A5%E6%9C%80%E5%A4%A7%E9%99%90%E5%BA%A6%E5%A4%8D%E7%94%A8thread-of-control%EF%BC%8C%E8%AE%A9%E4%B8%80%E4%B8%AA%E7%BA%BF%E7%A8%8B%E8%83%BD%E6%9C%8D%E5%8A%A1%E4%BA%8E%E5%A4%9A%E4%B8%AAsocket%E8%BF%9E%E6%8E%A5%E3%80%82%20IO%E7%BA%BF%E7%A8%8B%E5%8F%AA%E8%83%BD%E9%98%BB%E5%A1%9E%E5%9C%A8IO-multiplexing%E5%87%BD%E6%95%B0%E4%B8%8A%EF%BC%8C%E5%A6%82select%20%28%29%2Fpoll,%28%29%2Fepoll_wait%20%28%29%E3%80%82%20%E8%BF%99%E6%A0%B7%E4%B8%80%E6%9D%A5%EF%BC%8C%E5%BA%94%E7%94%A8%E5%B1%82%E7%9A%84%E7%BC%93%E5%86%B2%E6%98%AF%E5%BF%85%E9%A1%BB%E7%9A%84%EF%BC%8C%E6%AF%8F%E4%B8%AATCP%20socket%E9%83%BD%E8%A6%81%E6%9C%89input%20buffer%E5%92%8Coutput%20buffer%E3%80%82%20TcpConnection%E5%BF%85%E9%A1%BB%E6%9C%89output%20buffer)

