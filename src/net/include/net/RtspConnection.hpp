#pragma once
#include "net/MsgNode.hpp"
#include "net/RtpConnection.hpp"
#include "Rtp.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <net/media.hpp>
#include <queue>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
class RtspServer;
enum class Method {
    OPTIONS = 0,
    DESCRIBE,
    SETUP,
    PLAY,
    TEARDOWN,
    GET_PARAMETER,
    RTCP,
    NONE,
};

enum class ConnectionState {
    START_CONNECT,
    START_PLAY,
    START_PUSH
};

class RtspConnect : public std::enable_shared_from_this<RtspConnect> {
public:
    RtspConnect(std::shared_ptr<RtspServer> server,
                boost::asio::io_context &ioc);

    void AsyncRead();
    bool ParseRequest(char const *buffer);
    bool HandleRequest();
    void HandleWrite(boost::system::error_code const &ec, std::size_t size,
                     std::shared_ptr<RtspConnect> con_);
    void Send(char const *buffer, size_t size);
    void Send(std::string const &str);
    uint16_t GetRtpPort();
    uint16_t GetRtcpPort();

    u_int32_t GetCSeq();

    inline boost::asio::ip::tcp::socket &GetSocket() {
        return socket_;
    }

    inline Method GetMethod() {
        return method_;
    }

    inline TransportMode GetTransport() {
        return transport_;
    }

    inline MediaChannelID GetChannelId() {
        return channelid_;
    }

    inline std::string GetIp() const {
        return socket_.remote_endpoint().address().to_string();
    }

    inline uint16_t GetPort() const {
        return socket_.remote_endpoint().port();
    }

    std::string GetRtspUrl() const;

    std::string GetRtspUrlSuffix() const;

    u_int8_t GetRtpChannel() const;
    u_int8_t GetRtcpChannel() const;

    std::string GetSocketIp(boost::asio::ip::tcp::socket &socket);

private:
    friend class RtpConnect;
    std::weak_ptr<RtspServer> server_;
    boost::asio::ip::tcp::socket socket_;

    std::queue<std::shared_ptr<msgNode>> send_que_;
    std::queue<std::shared_ptr<msgNode>> recv_que_;
    std::shared_ptr<Recv_Node> recv_node_;
    std::shared_ptr<Send_Node> send_node_;
    std::mutex send_mtx_;

    // rtp
    std::shared_ptr<RtpConnect> rtp_conn_;

    std::unordered_map<std::string, std::pair<std::string, int>>
        Request_line_parmas_;
    std::unordered_map<std::string, std::pair<std::string, int>>
        Header_line_parmas_;
    Method method_;
    std::string URL_;
    TransportMode transport_;
    MediaChannelID channelid_;

    ConnectionState conn_state_ = ConnectionState::START_CONNECT;
    MediaSessionId session_id_ = 0;

private:
    bool ParseRequestLine(std::string &line);
    bool ParseHeaderLine(std::string &line);
    bool ParseTransport(std::string &line);
    bool ParseCSeq(std::string &line);
    bool ParseAccept(std::string &line);
    bool ParseMediaChannel(std::string &line);
    bool ParseSessionId(std::string &line);

    // handle Rtsp_cmd
    void HandleOptions();
    void HandleDescribe();
    void HandleSetup();
    void HandlePlay();
    void HandleRtcp();

    // build response
    int BuildOptions_res(char const *res, size_t size);
    int BuildDescribe_res(char const *res, size_t size, std::string sdp);
    int BuildSetupTcp_res(char const *res, size_t size, uint16_t rtp_chn,
                          uint16_t rtcp_chn, uint32_t session_id);
    int BuildSetupUdp_res(char const *res, size_t size, uint16_t rtp_chn,
                          uint16_t rtcp_chn, uint32_t session_id);
    int BuildPlay_res(char const *res, size_t size, char const *rtpInfo,
                      uint32_t session_id);
    int BuildNotFound_res(char const *res, int size);
    int BuildServerError_res(char const *res, int size);
};
