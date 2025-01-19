#pragma once

#include "media.hpp"
#include "net/SingleTon.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <net/MediaSource.hpp>
#include <net/RingBuffer.hpp>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <mutex>



class  RtpConnect;
class MediaSession : public SingleTon<MediaSession> {
    using NotifyConnectedCallback = std::function<void(
        MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port)>;
    using NotifyDisconnectedCallback = std::function<void(
        MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port)>;
    friend class SingleTon<MediaSession>;
public:
    virtual ~MediaSession();

    bool AddSource(MediaChannelID media_channel_id, MediaSource *source);
    bool RemoveSource(MediaChannelID media_channel_id);

    void AddNotifyConnectedCallback(const NotifyConnectedCallback& callback);
    void AddNotifyDisconnectedCallback(const NotifyDisconnectedCallback &callback);

    std::string GetRtspUrlSuffix() const
	{ return suffix_;
    }

    void SetRtspUrlSuffix(std::string &suffix) {
        suffix_ = suffix;
    }

    MediaSessionId GetMediaSessionId() const {
        return session_id_;
    }

    uint32_t GetNumClient() const
	{ return (uint32_t)clients_.size(); }

	std::string GetSdpMessage(std::string ip, std::string session_name ="");

	MediaSource* GetMediaSource(MediaChannelID channel_id);

	bool HandleFrame(MediaChannelID channel_id, AVFrame frame);

	bool AddClient(std::shared_ptr<RtpConnect> rtp_conn);
	void RemoveClient(std::shared_ptr<RtpConnect> rtp_conn);

private:
    MediaSession(std::string url_suffix);
    MediaSessionId session_id_ = 0;
    std::string suffix_;
    std::string sdp_;
    std::vector<std::unique_ptr<MediaSource>> media_sources_;
	std::vector<RingBuffer<AVFrame>> buffer_;
	std::vector<NotifyConnectedCallback> notify_connected_callbacks_;
	std::vector<NotifyDisconnectedCallback> notify_disconnected_callbacks_;
    std::atomic<bool> has_new_client_;
    static std::atomic_uint last_session_id_;


    std::mutex mutex_;
	std::mutex client_mutex_;
	std::vector< std::weak_ptr<RtpConnect>> clients_;
    
};
