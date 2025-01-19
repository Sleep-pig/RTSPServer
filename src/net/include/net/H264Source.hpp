#pragma once

#include "net/media.hpp"
#include "net/MediaSource.hpp"
#include "net/SingleTon.hpp"
#include <cstdint>

class H264Source : public MediaSource, public SingleTon<H264Source> {
    friend class SingleTon<H264Source>;

public:
    ~H264Source();

    void SetFramerate(uint32_t framerate) {
        framerate_ = framerate;
    }

    uint32_t GetFramerate() const {
        return framerate_;
    }

    virtual std::string GetMediaDescription(uint16_t) override;
    virtual std::string GetAttribute() override;
    virtual bool HandleFrame(MediaChannelID channel_id, AVFrame frame) override;
    static uint32_t GetTimeStamp();

private:
    H264Source(uint32_t framerate = 25);
    uint32_t framerate_;
};
