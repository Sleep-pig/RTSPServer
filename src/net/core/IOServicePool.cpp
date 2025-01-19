#include <memory>
#include <net/IOServicePool.hpp>

IOServicePool::IOServicePool(std::size_t size)
    : services_(size),
      works_(size),
      next_index_(0) {
    for (int i = 0; i < size; i++) {
        works_[i] = std::unique_ptr<Work>(new Work(services_[i]));
    }

    for (int i = 0; i < size; i++) {
        threads_.emplace_back([this, i] { services_[i].run(); });
    }
}

IOService &IOServicePool::GetService() {
    auto &service = services_[next_index_];
    if (next_index_++ == services_.size()) {
        next_index_ = 0;
    }
    return service;
}

IOServicePool::~IOServicePool() {
    for (auto& work_: works_) {
        work_.reset();
    }

    for (auto& it: threads_) {
        it.join();
    }
}
