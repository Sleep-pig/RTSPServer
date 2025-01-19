#include "Log/logger.hpp"
#include "net/IOServicePool.hpp"
#include "net/media.hpp"
#include "net/MediaSession.hpp"
#include "net/RtspConnection.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/detail/error_code.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <net/RtspServer.hpp>
#include <ostream>

RtspServer::RtspServer(boost::asio::io_context &ioc, short port)
    : acceptor_(ioc, tcp::endpoint(tcp::v4(), port)),
      ioc_(ioc) {}

void RtspServer::Start() {
    auto self = shared_from_this();
    auto &ioc = IOServicePool::GetInstance()->GetService();
    std::shared_ptr<RtspConnect> new_con =
        std::make_shared<RtspConnect>(shared_from_this(), ioc);
    connections_.emplace_back(new_con);
    acceptor_.async_accept(
        new_con->GetSocket(),
        [new_con, self](boost::system::error_code const &ec) {
            try {
                if (ec) {
                    self->Start();
                    return;
                }
                LOG_DEBUG("new connection");
                new_con->AsyncRead();
                self->Start();
            } catch (std::exception &exp) {
                std::cout << "error:" << exp.what() << std::endl;
                self->Start();
            }
        });
}

MediaSessionId RtspServer::AddSession(MediaSession *session) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (rtsp_suffix_map_.find(session->GetRtspUrlSuffix()) !=
        rtsp_suffix_map_.end()) {
        return 0;
    }

    std::shared_ptr<MediaSession> media_session(session);
    MediaSessionId id = media_session->GetMediaSessionId();
    rtsp_suffix_map_.emplace(std::move(media_session->GetRtspUrlSuffix()), id);
    media_sessions_.emplace(id, std::move(media_session));
    return id;
}

void RtspServer::RemoveSession(MediaSessionId id) {
    std::lock_guard<std::mutex> locker(mtx_);

    auto iter = media_sessions_.find(id);
    if (iter != media_sessions_.end()) {
        rtsp_suffix_map_.erase(iter->second->GetRtspUrlSuffix());
        media_sessions_.erase(id);
    }
}

bool RtspServer::PushFrame(MediaSessionId id, MediaChannelID channel,
                           AVFrame frame) {
    std::shared_ptr<MediaSession> session = nullptr;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto iter = media_sessions_.find(id);
        if(iter != media_sessions_.end()) {
            session = iter->second;
        } else {
            return false;
        }
    }

    if (session != nullptr && session->GetNumClient() != 0) {
        return session->HandleFrame(channel, frame);
    }
    return false;
}

std::shared_ptr<MediaSession>
RtspServer::LookMediaSession(std::string const &suffix) {
    std::lock_guard<std::mutex> lk(mtx_);

    auto iter = rtsp_suffix_map_.find(suffix);
    if (iter != rtsp_suffix_map_.end()) {
        MediaSessionId id = iter->second;
        return media_sessions_[id];
    }
    return nullptr;
}

std::shared_ptr<MediaSession> RtspServer ::LookMediaSession(MediaSessionId id) {
    std::lock_guard<std::mutex> locker(mtx_);

    auto iter = media_sessions_.find(id);
    if (iter != media_sessions_.end()) {
        return iter->second;
    }

    return nullptr;
}

RtspServer ::~RtspServer() {}
