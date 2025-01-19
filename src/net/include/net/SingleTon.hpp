#pragma once

#include <memory>
#include <mutex>

template <typename T>

class SingleTon {
public:
    template<typename ...Args>
    static std::shared_ptr<T> &GetInstance(Args ...args) {
        static std::once_flag flag;
        std::call_once(flag, [&] { instance_ = std::shared_ptr<T>(new T(args...)); });
        return instance_;
    }

    static std::shared_ptr<T> instance_;

protected:
    SingleTon() = default;
    ~SingleTon() = default;
    SingleTon(SingleTon const &) = delete;
    SingleTon &operator=(SingleTon const &) = delete;
};

template <typename T>

std::shared_ptr<T> SingleTon<T>::instance_ = nullptr;