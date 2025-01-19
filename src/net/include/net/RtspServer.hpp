#pragma once

#include "net/MediaSession.hpp"
#include "net/media.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
class RtspConnect;
using tcp = boost::asio::ip::tcp;

class RtspServer : public std::enable_shared_from_this<RtspServer> {
    friend class RtspConnect;

public:
     RtspServer(boost::asio::io_context &ioc, short port);
    void Start();
    MediaSessionId AddSession(MediaSession *session);
    void RemoveSession(MediaSessionId id);
    bool PushFrame(MediaSessionId id, MediaChannelID channel, AVFrame frame);

    inline void SetVersion(std::string const &version) { //SDP session name
        version_ = version;
    }

    inline std::string GetVersion() { return version_; }


    
    ~RtspServer();

private:
    std::shared_ptr<MediaSession> LookMediaSession(const std::string& suffix);
    std::shared_ptr<MediaSession> LookMediaSession(MediaSessionId id);
    tcp::acceptor acceptor_;
    boost::asio::io_context &ioc_;
    std::vector<std::shared_ptr<RtspConnect>> connections_;

    std::mutex mtx_;
    std::unordered_map<MediaSessionId, std::shared_ptr<MediaSession>>
        media_sessions_;
    std::unordered_map<std::string, MediaSessionId> rtsp_suffix_map_;

    std::string version_;
};
