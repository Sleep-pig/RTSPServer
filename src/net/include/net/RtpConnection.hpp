#pragma once

#include "net/LogicSystem.hpp"
#include "net/media.hpp"
#include "net/Rtp.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>


class RtspConnect;
struct endpoint {
    boost::asio::ip::address addr;
    uint16_t port;
};

using UdpSocketPtr = std::unique_ptr<boost::asio::ip::udp::socket>;

class RtpConnect : public std::enable_shared_from_this<RtpConnect> {
    friend class LogicSystem;
public:
    RtpConnect(std::shared_ptr<RtspConnect> con);
    RtpConnect()=default;
    bool RtcpAsyncRead(MediaChannelID channel_id);

    inline void SetClockRate(MediaChannelID channel_id, uint32_t clock_rate) {
        media_channel_info_[channel_id].clock_rate = clock_rate;
    }

    inline void SetPayloadType(MediaChannelID channel_id, uint32_t payload) {
        media_channel_info_[channel_id].rtp_header.payload = payload;
    }

    inline uint32_t GetRtpSessionId() {
        return (uint32_t)((size_t)(this));
    }

    inline uint16_t GetRtpPort(MediaChannelID channel_id) {
        return local_rtp_ports[channel_id];
    }

    inline uint16_t GetRtcpPort(MediaChannelID channel_id) {
        return local_rtcp_ports[channel_id];
    }

    inline int GetId() const {
        return static_cast<int>((size_t)(this));
    }

    inline uint16_t GetPort() {
        return rtsp_port_;
    }

    inline std::string GetIp() {
        return rtsp_ip_;
    }

    bool SetupRtpOverUdp(MediaChannelID channel_id, uint16_t rtp_port,
                         uint16_t rtcp_port);

    bool SetupRtpOverTcp(MediaChannelID channel_id,uint16_t rtp_channel, uint16_t rtcp_channel);
    void Play();
    void TearDown();

    int SendRtpPacket(MediaChannelID channel_id, RtpPacket pkt);

private:
    char buffer[2048];
    std::weak_ptr<RtspConnect> rtsp_con_;
    boost::asio::ip::tcp::endpoint peer_endpoint_;
    //udp
    endpoint peer_rtp_addr_[MAX_MEDIA_CHANNEL];
    endpoint peer_rtcp_addr_[MAX_MEDIA_CHANNEL];
    uint16_t local_rtp_ports[MAX_MEDIA_CHANNEL];
    uint16_t local_rtcp_ports[MAX_MEDIA_CHANNEL];
    UdpSocketPtr rtp_sockets_[MAX_MEDIA_CHANNEL];
    UdpSocketPtr rtcp_sockets_[MAX_MEDIA_CHANNEL];
    //tcp
    std::unique_ptr<boost::asio::ip::tcp::socket> tcp_socket_;

    TransportMode transport_mode_;
    mediaChannelInfo media_channel_info_[MAX_MEDIA_CHANNEL];

    std::string rtsp_ip_;
    uint16_t rtsp_port_;

    bool is_multicast_ = false;
    bool is_closed_ = false;
    bool has_key_frame_ = false;

    uint8_t frame_type_ = 0;

private:
    void HandleRead_Rtcp(boost::system::error_code const &ec, size_t bytes,
                         std::shared_ptr<RtpConnect> con,
                         MediaChannelID channel_id);


    void SetFrameType(uint8_t frame_type);
    void SetRtpHeader(MediaChannelID channel_id, RtpPacket pkt);
    int SendRtpOverTcp(MediaChannelID channel_id, RtpPacket pkt);
    int SendRtpOverUdp(MediaChannelID channel_id, RtpPacket pkt);
    
};
