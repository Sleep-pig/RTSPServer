#include "Log/logger.hpp"
#include "net/const.hpp"
#include "net/IOServicePool.hpp"
#include "net/LogicSystem.hpp"
#include "net/media.hpp"
#include "net/MsgNode.hpp"
#include "net/Rtp.hpp"
#include "net/RtspConnection.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <net/RtpConnection.hpp>
#include <netinet/in.h>
#include <random>
#include <string>

RtpConnect::RtpConnect(std::shared_ptr<RtspConnect> con)
    : rtsp_con_(con),
      tcp_socket_(nullptr) {
    std::random_device rd;
    for (int chn = 0; chn < MAX_MEDIA_CHANNEL; ++chn) {
        rtp_sockets_[chn] = nullptr;
        rtcp_sockets_[chn] = nullptr;
        memset(&media_channel_info_[chn], 0, sizeof(media_channel_info_[chn]));
        media_channel_info_[chn].rtp_header.version = RTP_VERSION;
        media_channel_info_[chn].packet_seq = rd() & 0xffff;
        media_channel_info_[chn].rtp_header.seq = 0;
        media_channel_info_[chn].rtp_header.ts = htonl(rd());
        media_channel_info_[chn].rtp_header.ssrc = htonl(rd());
    }
    auto conn = rtsp_con_.lock();
    rtsp_ip_ = conn->GetIp();
    rtsp_port_ = conn->GetPort();
}

bool RtpConnect::RtcpAsyncRead(MediaChannelID channel_id) {
    rtcp_sockets_[channel_id]->async_receive(
        boost::asio::buffer(buffer, sizeof(buffer)),
        std::bind(&RtpConnect::HandleRead_Rtcp, this, std::placeholders::_1,
                  std::placeholders::_2, shared_from_this(), channel_id));
    return true;
}

bool RtpConnect::SetupRtpOverUdp(MediaChannelID channel_id, uint16_t rtp_port,
                                 uint16_t rtcp_port) {
    auto con = rtsp_con_.lock();
    if (!con) {
        LOG_DEBUG("cannot get rtsp_con");
        return false;
    }

    peer_endpoint_ = con->socket_.remote_endpoint();
    media_channel_info_[channel_id].rtp_port = rtp_port;
    media_channel_info_[channel_id].rtcp_port = rtcp_port;

    std::random_device rd;
    for (int i = 0; i < 10; ++i) {
        if (i == 10) {
            return false;
        }
        local_rtp_ports[channel_id] = rd() % 0xfffe;
        local_rtcp_ports[channel_id] = local_rtp_ports[channel_id] + 1;

        auto &rtp_ioc = IOServicePool::GetInstance()->GetService();
        rtp_sockets_[channel_id] =
            std::make_unique<boost::asio::ip::udp::socket>(
                rtp_ioc, boost::asio::ip::udp::v4());
        boost::system::error_code ec;
        try {
            rtp_sockets_[channel_id]->bind(boost::asio::ip::udp::endpoint(
                boost::asio::ip::udp::v4(), local_rtp_ports[channel_id]));
        } catch (boost::system::error_code &ec) {
            rtp_sockets_[channel_id].reset();
            LOG_DEBUG("bind rtp port failed: %s", ec.message().c_str());
            continue;
        }

        auto &rtcp_ioc = IOServicePool::GetInstance()->GetService();
        rtcp_sockets_[channel_id] =
            std::make_unique<boost::asio::ip::udp::socket>(
                rtcp_ioc, boost::asio::ip::udp::v4());
        try {
            rtcp_sockets_[channel_id]->bind(boost::asio::ip::udp::endpoint(
                boost::asio::ip::udp::v4(), local_rtcp_ports[channel_id]));
        } catch (boost::system::error_code &ec) {
            rtp_sockets_[channel_id].reset();
            rtcp_sockets_[channel_id].reset();
            LOG_DEBUG("bind rtcp port failed: %s", ec.message().c_str());
            continue;
        }
        break;
    }

    peer_rtp_addr_[channel_id].addr = peer_endpoint_.address().to_v4();
    peer_rtp_addr_[channel_id].port = media_channel_info_[channel_id].rtp_port;
    peer_rtcp_addr_[channel_id].addr = peer_endpoint_.address().to_v4();
    peer_rtcp_addr_[channel_id].port =
        media_channel_info_[channel_id].rtcp_port;
    media_channel_info_[channel_id].is_setup = true;
    transport_mode_ = TransportMode::RTP_OVER_UDP;
    return true;
}

bool RtpConnect::SetupRtpOverTcp(MediaChannelID channel_id,
                                 uint16_t rtp_channel, uint16_t rtcp_channel) {
    auto con = rtsp_con_.lock();
    if (!con) {
        LOG_DEBUG("cannot get rtsp_con");
        return false;
    }
    media_channel_info_[channel_id].rtp_channel = rtp_channel;
    media_channel_info_[channel_id].rtcp_channel = rtcp_channel;
    media_channel_info_[channel_id].is_setup = true;
    transport_mode_ = TransportMode::RTP_OVER_TCP;
    return true;
}

void RtpConnect::Play() {
    for (int i = 0; i < MAX_MEDIA_CHANNEL; i++) {
        if (media_channel_info_[i].is_setup) {
            media_channel_info_[i].is_play = true;
        }
    }
}

void RtpConnect::TearDown() {
    if (!is_closed_) {
        is_closed_ = true;
        for (int chn = 0; chn < MAX_MEDIA_CHANNEL; chn++) {
            media_channel_info_[chn].is_play = false;
            media_channel_info_[chn].is_record = false;
        }
    }
}

int RtpConnect::SendRtpPacket(MediaChannelID channel_id, RtpPacket pkt) {
    if (is_closed_) {
        return -1;
    }

    auto rtsp_conn = rtsp_con_.lock();
    if (!rtsp_conn) {
        return -1;
    }

    this->SetFrameType(pkt.type);
    this->SetRtpHeader(channel_id, pkt);
    int ret = 0;
    if ((media_channel_info_[channel_id].is_play ||
         media_channel_info_[channel_id].is_record) &&
        has_key_frame_) {
        if (transport_mode_ == TransportMode::RTP_OVER_UDP) {
            ret = SendRtpOverUdp(channel_id, pkt);
        } else {
            ret = SendRtpOverTcp(channel_id, pkt);
        }
    }

    return ret == 0 ? 0 : -1;
}

void RtpConnect::HandleRead_Rtcp(boost::system::error_code const &ec,
                                 size_t bytes, std::shared_ptr<RtpConnect> con,
                                 MediaChannelID channel_id) {
    if (ec) {
        LOG_DEBUG("read rtcp failed: {}", ec.message().c_str());
        con->RtcpAsyncRead(channel_id);
        return;
    } else {
        LOG_DEBUG("msg is:%s", con->buffer);
        con->RtcpAsyncRead(channel_id);
    }
}

void RtpConnect::SetFrameType(uint8_t frame_type) {
    frame_type_ = frame_type;
    if (!has_key_frame_ &&
        (frame_type == 0 || frame_type == FrameType::VIDEO_FRAME_I)) {
        has_key_frame_ = true;
    }
}

void RtpConnect::SetRtpHeader(MediaChannelID channel_id, RtpPacket pkt) {
    if ((media_channel_info_[channel_id].is_play ||
         media_channel_info_[channel_id].is_record) &&
        has_key_frame_) {
        media_channel_info_[channel_id].rtp_header.marker = pkt.last;
        media_channel_info_[channel_id].rtp_header.ts = htonl(pkt.timestamp);
        media_channel_info_[channel_id].rtp_header.seq =
            htons(media_channel_info_[channel_id].packet_seq++);
        memcpy(pkt.data.get() + 4, &media_channel_info_[channel_id].rtp_header,
               RTP_HEADER_SIZE);
    }
}

int RtpConnect::SendRtpOverTcp(MediaChannelID channel_id, RtpPacket pkt) {
    auto conn = rtsp_con_.lock();
    if (!conn) {
        return -1;
    }

    uint8_t *rtpPktPtr = pkt.data.get();
    rtpPktPtr[0] = '$'; // 多4个byte 第一个固定为0x24  第二个为通道号  三四
                        // 为除了前四个的长度
    rtpPktPtr[1] = (char)(media_channel_info_[channel_id].rtp_channel);
    rtpPktPtr[2] = (char)(((pkt.size - 4) & 0xFF00) >> 8);
    rtpPktPtr[3] = (char)((pkt.size - 4) & 0xFF);
    std::shared_ptr<Send_Node> node =
        std::make_shared<Send_Node>((char *)rtpPktPtr, pkt.size);
    node->id_ = MSG_IDS::RTP_SEND_PKT;
    LogicSystem::GetInstance()->PushMsg(
        std::make_shared<LogicNode>(conn, node));
    // boost::asio::const_buffers_1 buffers(rtpPktPtr, pkt.size);
    // conn->GetSocket().async_send(
    //     boost::asio::buffer(rtpPktPtr, pkt.size),
    //     [this](boost::system::error_code ec, std::size_t bytes) {
    //         std::cout<<"send size:"<<bytes<<"non err\n";
    //         if (ec) {
    //             TearDown();
    //             LOG_DEBUG("send rtp failed: %s", ec.message().c_str());
    //         }
    //     });
    return 0;
}

int RtpConnect::SendRtpOverUdp(MediaChannelID channel_id, RtpPacket pkt) {
    rtp_sockets_[channel_id]->async_send_to(
        boost::asio::buffer(pkt.data.get() + 4, pkt.size - 4),
        boost::asio::ip::udp::endpoint(peer_rtp_addr_[channel_id].addr.to_v4(),
                                       peer_rtp_addr_[channel_id].port),
        [this](boost::system::error_code ec, std::size_t bytes) {
            if (ec) {
                TearDown();
                LOG_DEBUG("send rtp failed: %s", ec.message().c_str());
            }
        });
    return 1;
}
