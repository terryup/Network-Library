#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H

/*
禁止拷贝操作的基类，设置为protect权限的成员函数可以让派生类继承
派生类可以正常的构造和析构
*/


class noncopyable{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;

};



#endif