#include "Log/logger.hpp"
#include "net/Rtp.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <forward_list>
#include <memory>
#include <mutex>
#include <net/media.hpp>
#include <net/MediaSession.hpp>
#include <net/RtpConnection.hpp>

std::atomic_uint MediaSession::last_session_id_(1);

MediaSession::MediaSession(std::string url_suffix)
    : suffix_(url_suffix),
      buffer_(MAX_MEDIA_CHANNEL),
      media_sources_(MAX_MEDIA_CHANNEL) {
    has_new_client_ = false;
    session_id_ = ++last_session_id_;
    sdp_ = "";
}

MediaSession::~MediaSession() {}

bool MediaSession::AddSource(MediaChannelID media_channel_id,
                             MediaSource *source) {
    source->SetSendFrameCallback([this](MediaChannelID channel_id,
                                        RtpPacket packet) -> bool {
        std::forward_list<std::shared_ptr<RtpConnect>> clients;
        std::map<int, RtpPacket> packets;
        {
            std::lock_guard<std::mutex> lock(client_mutex_);
            for (auto iter = clients_.begin(); iter != clients_.end();) {
                auto conn = iter->lock();
                if (conn == nullptr) {
                    clients_.erase(iter++);
                } else {
                    int id = conn->GetRtpSessionId();
                    // if (id >= 0) {
                    if (packets.find(id) == packets.end()) {
                        RtpPacket pkt;
                        memcpy(pkt.data.get(), packet.data.get(), packet.size);
                        pkt.size = packet.size;
                        pkt.timestamp = packet.timestamp;
                        pkt.type = packet.type;
                        pkt.last = packet.last;
                        packets.emplace(id, pkt);
                    }
                    clients.emplace_front(conn);
                    //}
                    iter++;
                }
            }

            for (auto &iter: clients) {
                int id = iter->GetRtpSessionId();
                if (packets.find(id) != packets.end()) {
                    int ret = iter->SendRtpPacket(channel_id, packets[id]);
                    if (ret == 0) {
                        break;
                    }
                }
            }
        }
        return true;
    });
    media_sources_[media_channel_id].reset(source);
    return true;
}

bool MediaSession::RemoveSource(MediaChannelID media_channel_id) {
    media_sources_[media_channel_id] = nullptr;
    return true;
}

bool MediaSession::HandleFrame(MediaChannelID channel_id, AVFrame frame) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (media_sources_[channel_id]) {
        media_sources_[channel_id]->HandleFrame(channel_id, frame);
    } else {
        return false;
    }

    return true;
}

std::string MediaSession::GetSdpMessage(std::string ip,
                                        std::string session_name) {
    if (sdp_ != "") {
        return sdp_;
    }

    if (media_sources_.empty()) {
        return "";
    }

    char buff[2048] = {0};
    snprintf(buff, sizeof(buff),
             "o=- 9%ld 1 IN IP4 %s\r\n"
             "t=0 0\r\n"
             "a=control:*\r\n",
             (long)std::time(nullptr), ip.c_str());
    if (session_name != "") {
        snprintf(buff, sizeof(buff) - strlen(buff),
                 "a=type:broadcast\r\n"
                 "a=rtcp-unicast:reflection\r\n");
    }

    for (uint32_t chn = 0; chn < media_sources_.size(); chn++) {
        if (media_sources_[chn]) {
            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff), "%s\r\n",
                     media_sources_[chn]->GetMediaDescription(0).c_str());

            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff), "%s\r\n",
                     media_sources_[chn]->GetAttribute().c_str());

            snprintf(buff + strlen(buff), sizeof(buff) - strlen(buff),
                     "a=control:track%d\r\n", chn);
        }
    }

    sdp_ = buff;
    return sdp_;
}

MediaSource *MediaSession::GetMediaSource(MediaChannelID channel_id) {
    if (media_sources_[channel_id]) {
        return media_sources_[channel_id].get();
    }
    return nullptr;
}

void MediaSession::AddNotifyConnectedCallback(
    NotifyConnectedCallback const &callback) {
    notify_connected_callbacks_.push_back(callback);
}

void MediaSession::AddNotifyDisconnectedCallback(
    NotifyDisconnectedCallback const &callback) {
    notify_disconnected_callbacks_.push_back(callback);
}

bool MediaSession::AddClient(std::shared_ptr<RtpConnect> rtp_conn) {
    std::lock_guard<std::mutex> lk(client_mutex_);
    for (auto &iter: clients_) {
        if (iter.lock() == rtp_conn) {
            return false;
        }
    }
    std::weak_ptr<RtpConnect> rtp_conn_weak_ptr = rtp_conn;

    clients_.emplace_back(rtp_conn_weak_ptr);
    for (auto &callback: notify_connected_callbacks_) {
        callback(session_id_, rtp_conn->GetIp(), rtp_conn->GetPort());
    }

    has_new_client_ = true;
    return true;
}

void MediaSession::RemoveClient(std::shared_ptr<RtpConnect> rtp_conn) {
    std::lock_guard<std::mutex> lk(client_mutex_);
    for (auto iter = clients_.begin(); iter != clients_.end();) {
        if (iter->lock() == rtp_conn) {
            auto conn = iter->lock();
            if (conn) {
                for (auto &callback: notify_disconnected_callbacks_) {
                    callback(session_id_, conn->GetIp(), conn->GetPort());
                }
            }
            iter = clients_.erase(iter);
            return;
        } else {
            ++iter;
        }
    }
}
