#pragma once

#include "const.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

class RtspConnect;
enum class Method;

static void MyPrintf(char const *msg) {
    printf("%x\n", *msg);
    printf("%x\n", *(msg + 1));
    printf("%x\n", *(msg + 2));
    printf("%x\n", *(msg + 3));
}

class msgNode {
    friend class RtspConnect;

public:
    msgNode(size_t size) : current_len_(0), total_len_(size) {
        data_ = new char[size + 1];
        data_[size] = '\0';
    }

    void Clear() {
        current_len_ = 0;
        memset(data_, 0, total_len_);
    }

    ~msgNode() {
        delete[] data_;
    }

    inline char *Getdata() {
        return data_;
    }

    inline size_t GetLen() {
        return total_len_;
    }

    MSG_IDS id_;

protected:
    size_t current_len_;
    size_t total_len_;
    char *data_;
};

class Recv_Node : public msgNode {
public:
    Recv_Node(size_t size) : msgNode(size) {}
};

class Send_Node : public msgNode {
public:
    Send_Node(char const *data, size_t size) : msgNode(size) {
        memcpy(data_, data, size);
    }
};
