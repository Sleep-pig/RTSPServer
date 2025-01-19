#include "net/H264Source.hpp"
#include "Log/logger.hpp"
#include "net/media.hpp"
#include "net/Rtp.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/types.h>

H264Source::H264Source(uint32_t framerate) : framerate_(framerate) {
    payload_ = 96;
    type_ = MediaType::H264;
    clock_rate_ = 90000;
}

H264Source::~H264Source() {}

std::string H264Source::GetMediaDescription(uint16_t port) {
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP 96", port); // \r\nb=AS:2000
    return std::string(buf);
}

std::string H264Source::GetAttribute() {
    return std::string("a=rtpmap:96 H264/90000");
}

uint32_t H264Source::GetTimeStamp() {
    auto time_point = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now());
    return (uint32_t)((time_point.time_since_epoch().count() + 500) / 1000 *
                      90);
    // 返回时间戳单位90000hz   +500微秒弥补时间误差  /1000 微秒转毫秒  *90
    // 毫秒转90000hz
}

bool H264Source::HandleFrame(MediaChannelID channel_id, AVFrame frame) {
    uint8_t *frame_buf = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
        frame.timestamp = GetTimeStamp();
    }

    if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
        RtpPacket rtp_pkt;
        rtp_pkt.type = frame.type;
        rtp_pkt.timestamp = frame.timestamp;
        rtp_pkt.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
        rtp_pkt.last = 1;

        memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE,
               frame_buf, frame_size);
        if (send_frame_cb_) {
            if (send_frame_cb_(channel_id, rtp_pkt) == false) {
                return false;
            }
        }
    } else {
        char FU[2] ={0};
        FU[0] = (frame_buf[0] & 0xE0) | 28;
        FU[1] = 0x80 | (frame_buf[0] & 0x1F);
        frame_buf += 1;
        frame_size -= 1;
        while (frame_size + 2 > MAX_RTP_PAYLOAD_SIZE) {
            RtpPacket rtp_pkt;
            rtp_pkt.type = frame.type;
            rtp_pkt.timestamp = frame.timestamp;
            rtp_pkt.size =
                MAX_RTP_PAYLOAD_SIZE + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
            rtp_pkt.last = 0;
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU[0];
            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU[1];
            memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2,
                   frame_buf, MAX_RTP_PAYLOAD_SIZE - 2);

            if (send_frame_cb_) {
                if (send_frame_cb_(channel_id, rtp_pkt) == false) {
                    return false;
                }
            }
            frame_buf += MAX_RTP_PAYLOAD_SIZE - 2;
            frame_size -= MAX_RTP_PAYLOAD_SIZE - 2;

            FU[1] &= ~0x80;
        }

        {
            // 分包的最后一个包
            RtpPacket rtp_pkt;
            rtp_pkt.type = frame.type;
            rtp_pkt.timestamp = frame.timestamp;
            rtp_pkt.size = frame_size + 2 + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
            rtp_pkt.last = 1;

            FU[1] |= 0x40;

            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 0] = FU[0];

            rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 1] = FU[1];
            memcpy(rtp_pkt.data.get() + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + 2,
                   frame_buf, frame_size);
            if (send_frame_cb_) {
                if (send_frame_cb_(channel_id, rtp_pkt) == false) {
                    return false;
                }
            }
        }
    }

    return true;
}
