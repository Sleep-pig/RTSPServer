#include <any>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <utility>
#include <condition_variable>

enum LogLv {
    DEBUGS = 0,
    INFO = 1,
    WARN = 2,
    ERRORS = 3,
};

class LogTask {
public:
    LogTask() { }
    LogTask(LogTask const& src)
        : level_(src.level_)
        , logdatas(src.logdatas)
    {
    }

    LogTask(LogTask const&& src)
        : level_(src.level_)
        , logdatas(std::move(src.logdatas))
    {
    }
    LogLv level_;
    std::queue<std::any> logdatas;
};

class AsyncLog {
public:
    static AsyncLog& getInstance()
    {
        static AsyncLog _ins;
        return _ins;
    }

    template <typename... Args>
    void AsyncWrite(LogLv level, Args... args) {  
        auto task = std::make_shared<LogTask>();
        (task->logdatas.push(std::any(std::forward<Args>(args))), ...);
        task->level_ = level;
        std::unique_lock<std::mutex> lk(mtx_);
        que_.push(task);
        bool empty = que_.size() == 1;
        lk.unlock();
        if (empty) {
            empty_cond_.notify_one();
        }
    }

    void Start()
    {
        for (;;) {
            std::unique_lock<std::mutex> lk(mtx_);
            empty_cond_.wait(lk, [this] {
                return !que_.empty() || is_stop_;
            });

            if (is_stop_) {
                return;
            }

            auto logtask = que_.front();
            que_.pop();
            lk.unlock();
            processTask(logtask);
        }
    }

    void processTask(std::shared_ptr<LogTask> task)
    {
        std::cout << "[" << Priority_str[task->level_] << "] ";
        auto head = task->logdatas.front();
        task->logdatas.pop();

        std::string formatstr = "";
        bool success = conver2Str(head, formatstr);
        if (!success) {
            return;
        }

        while (!task->logdatas.empty()) {
            auto data = task->logdatas.front();
            formatstr = formatString(formatstr, data);
            task->logdatas.pop();
        }

        std::cout << formatstr << std::endl;
    }

    bool conver2Str(const std::any& data, std::string& replacement)
    {
        std::ostringstream ss;
        if (data.type() == typeid(int)) {
            ss << std::any_cast<int>(data);
        } else if (data.type() == typeid(float)) {
            ss << std::any_cast<float>(data);
        } else if (data.type() == typeid(double)) {
            ss << std::any_cast<double>(data);
        } else if (data.type() == typeid(std::string)) {
            ss << std::any_cast<std::string>(data);
        } else if (data.type() == typeid(char*)) {
            ss << std::any_cast<char*>(data);
        } else if (data.type() == typeid(const char*)) {
            ss << std::any_cast<const char*>(data);
        } else {
            return false;
        }
        replacement = ss.str();
        return true;
    }

    template <typename... Args>
    std::string formatString(const std::string& format,
        Args&&... args)
    {
        std::string result = format;
        size_t pos = 0;
        auto replaceplaceHolder = [&](std::string placeholder, std::any& replacement) {
            std::string str_replacement = "";
            bool success = conver2Str(replacement, str_replacement);
            if (!success) {
                return;
            }
            size_t placeholderPos = result.find(placeholder, pos);
            if (placeholderPos != std::string::npos) {
                result.replace(placeholderPos, placeholder.length(), str_replacement);
                pos += placeholderPos + placeholder.length();
            } else {
                result = result + " " + str_replacement;
            }
        };

        (replaceplaceHolder("{}", args), ...);
        return result;
    }

    void Stop()
    {
        is_stop_ = true;
        empty_cond_.notify_one();
    }

    ~AsyncLog()
    {
        Stop();
        workthread->join();
        std::cout << "destruct Log\n";
    }

private:
    AsyncLog(AsyncLog const& others) = delete;
    AsyncLog(AsyncLog const&& others) = delete;
    AsyncLog& operator=(AsyncLog const& other) = delete;
    AsyncLog()
        : is_stop_(false)
    {
        workthread = new std::thread(&AsyncLog::Start, this);
    }

    bool is_stop_;
    std::mutex mtx_;
    std::condition_variable empty_cond_;
    std::queue<std::shared_ptr<LogTask>> que_;
    std::thread* workthread;

    const char* Priority_str[4] = {
        "DEBUGS",
        "INFO",
        "WARNING",
        "ERRORS",
    };
};