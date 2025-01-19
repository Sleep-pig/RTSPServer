#pragma once

#include "net/media.hpp"
#include "net/Rtp.hpp"
#include <cstdint>
#include <functional>
using SendFrameCallback = std::function<bool(MediaChannelID, RtpPacket)>;

class MediaSource {
public:
    MediaSource() {}

    virtual ~MediaSource() {}

    virtual MediaType GetMediaType() const {
        return type_;
    }

    virtual uint32_t GetPayload() const {
        return payload_;
    }

    virtual void SetSendFrameCallback(SendFrameCallback const &cb) {
        send_frame_cb_ = cb;
    }

    virtual uint32_t GetClockRate() const {
        return clock_rate_;
    }

    virtual std::string GetMediaDescription(uint16_t port = 0) = 0;

    virtual std::string GetAttribute() = 0;

    virtual bool HandleFrame(MediaChannelID channelId, AVFrame frame) = 0;

protected:
    MediaType type_ = MediaType::NONE;
    uint32_t payload_;
    uint32_t clock_rate_;
    MediaChannelID channel_id_;
    SendFrameCallback send_frame_cb_;
};
