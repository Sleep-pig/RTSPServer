#pragma once

#include "SingleTon.hpp"
#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

using IOService = boost::asio::io_context;
using Work = boost::asio::io_context::work;
using WorkPtr = std::unique_ptr<Work>;

class IOServicePool : public SingleTon<IOServicePool> {
public:
    friend class SingleTon<IOServicePool>;
    IOService &GetService();
    ~IOServicePool();
private:
    IOServicePool(std::size_t size = std::thread::hardware_concurrency());
    std::vector<std::thread> threads_;
    std::vector<IOService> services_;
    std::vector<WorkPtr> works_;
    size_t next_index_;
};
