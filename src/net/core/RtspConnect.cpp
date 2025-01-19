
#include "Log/logger.hpp"
#include "net/const.hpp"
#include "net/LogicSystem.hpp"
#include "net/media.hpp"
#include "net/MediaSession.hpp"
#include "net/MediaSource.hpp"
#include "net/MsgNode.hpp"
#include "net/Rtp.hpp"
#include "net/RtspServer.hpp"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <net/RtpConnection.hpp>
#include <net/RtspConnection.hpp>

char const *MethodToString[8] = {"OPTIONS",  "DESCRIBE",      "SETUP", "PLAY",
                                 "TEARDOWN", "GET_PARAMETER", "RTCP",  "NONE"};

RtspConnect::RtspConnect(std::shared_ptr<RtspServer> server,
                         boost::asio::io_context &ioc)
    : server_(server),
      socket_(ioc) {}

bool RtspConnect::ParseRequest(char const *buffer) {
    if (buffer[0] == '$') {
        method_ = Method::RTCP;
        return true;
    }

    bool ret;
    std::string message(buffer);
    size_t pos = message.find("\r\n");
    if (pos == std::string::npos) {
        return false;
    }
    // parse request line
    std::string line = message.substr(0, pos);
    ret = ParseRequestLine(line);
    // parse header line
    line = message.substr(pos + 2);
    ret = ParseHeaderLine(line);
    return ret;
}

bool RtspConnect::ParseRequestLine(std::string &line) {
    std::string method;
    std::string Url;
    std::string Version;
    std::stringstream ss(line);
    ss >> method >> Url >> Version;
    if (method == "OPTIONS") {
        method_ = Method::OPTIONS;
    } else if (method == "DESCRIBE") {
        method_ = Method::DESCRIBE;
    } else if (method == "SETUP") {
        method_ = Method::SETUP;
    } else if (method == "PLAY") {
        method_ = Method::PLAY;
    } else {
        method_ = Method::NONE;
    }

    // parse url
    if (std::strncmp(Url.c_str(), "rtsp://", 7) != 0) {
        return false;
    }
    URL_ = Url.substr(7);

    uint16_t port = 0;
    char ip[64] = {0};
    char suffix[64] = {0};

    if (sscanf(URL_.c_str(), "%[^:]:%hu/%s", ip, &port, suffix) == 3) {
    } else if (sscanf(URL_.c_str(), "%[^/]/%s", ip, suffix) == 2) {
        port = 554;
    } else {
        return false;
    }

    Request_line_parmas_["url"] = {Url, 0};
    Request_line_parmas_["url_ip"] = {ip, 0};
    Request_line_parmas_["url_port"] = {"", port};
    Request_line_parmas_["url_suffix"] = {suffix, 0};
    Request_line_parmas_["version"] = {Version, 0};
    Request_line_parmas_["method"] = {std::move(method), 0};

    return true;
}

bool RtspConnect::ParseHeaderLine(std::string &line) {
    if (!ParseCSeq(line)) {
        if (Header_line_parmas_.find("cseq") == Header_line_parmas_.end()) {
            return false;
        }
    }

    if (method_ == Method::DESCRIBE) {
        if (ParseAccept(line)) {
            return true;
        }
        LOG_DEBUG("error:parseAccept failed");
        return false;
    } else if (method_ == Method::SETUP) {
        if (ParseTransport(line)) {
            if (ParseMediaChannel(line)) {
                return true;
            } else {
                LOG_DEBUG("error:ParseMediaChannel failed");
                return false;
            }
        }
        LOG_DEBUG("error:parseTransport failed");
        return false;
    } else if (method_ == Method::PLAY) {
        if (ParseSessionId(line)) {
            return true;
        }
        return false;
    } else if (method_ == Method::OPTIONS) {
        return true;
    }
    return false;
}

bool RtspConnect::ParseCSeq(std::string &line) {
    auto pos = line.find("CSeq");
    uint32_t cseq;
    if (pos != std::string::npos) {
        sscanf(line.c_str() + pos, "%*[^:]: %u", &cseq);
        Header_line_parmas_["cseq"] = {"", cseq};
        return true;
    }

    return false;
}

bool RtspConnect::ParseTransport(std::string &line) {
    std::size_t pos = line.find("Transport");
    if (pos != std::string::npos) {
        if ((pos = line.find("RTP/AVP/TCP")) != std::string::npos) {
            transport_ = TransportMode::RTP_OVER_TCP;
            uint16_t rtpChannel = 0, rtcpChannel = 0;
            if (sscanf(line.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu",
                       &rtpChannel, &rtcpChannel) != 2) {
                return false;
            }
            Header_line_parmas_["rtp_channel"] = std::make_pair("", rtpChannel);
            Header_line_parmas_["rtcp_channel"] =
                std::make_pair("", rtcpChannel);
            return true;
        } else if ((pos = line.find("RTP/AVP")) != std::string::npos) {
            uint16_t rtp_port = 0, rtcp_port = 0;
            if ((pos = line.find("unicast")) != std::string::npos) {
                transport_ = TransportMode::RTP_OVER_UDP;
                if (sscanf(line.c_str() + pos, "%*[^;];%*[^=]=%hu-%hu",
                           &rtp_port, &rtcp_port) != 2) {
                    return false;
                }
            } else {
                return false;
            }
            Header_line_parmas_["rtp_port"] = {"", rtp_port};
            Header_line_parmas_["rtcp_port"] = {"", rtcp_port};
            return true;
        }
    }
    return false;
}

bool RtspConnect::ParseAccept(std::string &line) {
    if (line.find("Accept") == std::string::npos ||
        line.find("sdp") == std::string::npos) {
        return false;
    }
    return true;
}

bool RtspConnect::ParseMediaChannel(std::string &line) {
    channelid_ = MediaChannelID::channel0;
    auto it = Request_line_parmas_.find("url");
    if (it != Request_line_parmas_.end()) {
        std::size_t pos = it->second.first.find("track1");
        if (pos != std::string::npos) {
            channelid_ = MediaChannelID::channel1;
        }
    }
    return true;
}

bool RtspConnect::ParseSessionId(std::string &line) {
    size_t pos = line.find("Session");
    if (pos != std::string::npos) {
        uint32_t session_id = 0;
        if (sscanf(line.c_str() + pos, "%*[^:]: %u", &session_id) != 1) {
            return false;
        }
    }
    return true;
}

void RtspConnect::AsyncRead() {
    auto self = shared_from_this();
    recv_node_ = std::make_shared<Recv_Node>(2048);
    socket_.async_read_some(
        boost::asio::buffer(recv_node_->Getdata(), recv_node_->GetLen()),
        [self](boost::system::error_code const &ec,
               std::size_t byte_transform) {
            try {
                if (ec) {
                    LOG_DEBUG(ec.what().c_str());
                    return;
                }
                LOG_DEBUG("msg is:%s", self->recv_node_->Getdata());
                self->recv_node_->id_ = MSG_IDS::REQUEST;
                std::shared_ptr<LogicNode> node =
                    std::make_shared<LogicNode>(self, self->recv_node_);
                LogicSystem::GetInstance()->PushMsg(node);
                self->AsyncRead();
            } catch (std::exception &e) {
                LOG_DEBUG(e.what());
                return;
            }
        });
}

void RtspConnect::Send(char const *msg, size_t size) {
    bool pending = false;
    std::lock_guard<std::mutex> lk(send_mtx_);
    if (send_que_.size() > 0) {
        pending = true;
    }
    send_que_.push(std::make_shared<Send_Node>(msg, size));
    if (pending) {
        return;
    }
    // LOG_DEBUG("send msg is:%s", msg);
    boost::asio::async_write(
        socket_, boost::asio::buffer(msg, size),
        std::bind(&RtspConnect::HandleWrite, this, std::placeholders::_1,
                  std::placeholders::_2, shared_from_this()));
}

void RtspConnect::Send(std::string const &str) {
    Send(str.c_str(), str.length());
}

uint16_t RtspConnect::GetRtpPort() {
    uint16_t rtp_port = 0;
    if (Header_line_parmas_.find("rtp_port") != Header_line_parmas_.end()) {
        rtp_port = Header_line_parmas_["rtp_port"].second;
    }
    return rtp_port;
}

uint16_t RtspConnect::GetRtcpPort() {
    uint16_t rtcp_port = 0;
    if (Header_line_parmas_.find("rtcp_port") != Header_line_parmas_.end()) {
        rtcp_port = Header_line_parmas_["rtcp_port"].second;
    }
    return rtcp_port;
}

bool RtspConnect::HandleRequest() {
    switch (method_) {
    case Method::RTCP:     break;
    case Method::OPTIONS:  HandleOptions(); break;
    case Method::DESCRIBE: HandleDescribe(); break;
    case Method::SETUP:    HandleSetup(); break;
    case Method::PLAY:     HandlePlay(); break;
    default:               break;
    }

    return true;
}

void RtspConnect::HandleWrite(boost::system::error_code const &ec,
                              std::size_t size,
                              std::shared_ptr<RtspConnect> self_con_) {
    if (!ec) {
        std::lock_guard<std::mutex> lk(send_mtx_);
        send_que_.pop();
        if (!send_que_.empty()) {
            auto &msgnode = send_que_.front();
            boost::asio::async_write(
                socket_,
                boost::asio::buffer(msgnode->data_, msgnode->total_len_),
                std::bind(&RtspConnect::HandleWrite, this,
                          std::placeholders::_1, std::placeholders::_2,
                          self_con_));
        }
    } else {
        LOG_DEBUG(ec.what().c_str());
        return;
    }
}

u_int32_t RtspConnect::GetCSeq() {
    uint32_t cseq = 0;
    if (Header_line_parmas_.find("cseq") != Header_line_parmas_.end()) {
        cseq = Header_line_parmas_["cseq"].second;
    }
    return cseq;
}

std::string RtspConnect::GetRtspUrl() const {
    auto iter = Request_line_parmas_.find("url");
    if (iter != Request_line_parmas_.end()) {
        return iter->second.first;
    }

    return "";
}

std::string RtspConnect::GetRtspUrlSuffix() const {
    auto iter = Request_line_parmas_.find("url_suffix");
    if (iter != Request_line_parmas_.end()) {
        return iter->second.first;
    }

    return "";
}

uint8_t RtspConnect::GetRtpChannel() const {
    auto iter = Header_line_parmas_.find("rtp_channel");
    if (iter != Header_line_parmas_.end()) {
        return iter->second.second;
    }

    return 0;
}

uint8_t RtspConnect::GetRtcpChannel() const {
    auto iter = Header_line_parmas_.find("rtcp_channel");
    if (iter != Header_line_parmas_.end()) {
        return iter->second.second;
    }

    return 0;
}

std::string RtspConnect::GetSocketIp(boost::asio::ip::tcp::socket &socket) {
    boost::asio::ip::tcp::endpoint endpoint = socket.remote_endpoint();
    boost::asio::ip::address address = endpoint.address();
    return address.to_string();
}

void RtspConnect::HandleOptions() {
    char response[2048];
    int ret = 0;
    ret = BuildOptions_res(response, sizeof(response));
    if (ret <= 0) {
        LOG_DEBUG("error:buildOption failed");
        return;
    }
    Send(response, strlen(response));
}

void RtspConnect::HandleDescribe() {
    char response[2048];
    int ret = 0;
    std::shared_ptr<MediaSession> media_session = nullptr;

    if (rtp_conn_ == nullptr) {
        rtp_conn_.reset(new RtpConnect(shared_from_this()));
    }

    auto rtsp_server = server_.lock();
    if (rtsp_server) {
        media_session = rtsp_server->LookMediaSession(this->GetRtspUrlSuffix());
    }

    if (!rtsp_server && !media_session) {
        ret = BuildNotFound_res(response, sizeof(response));
    } else {
        session_id_ = media_session->GetMediaSessionId();
        media_session->AddClient(rtp_conn_);

        for (int chn = 0; chn < MAX_MEDIA_CHANNEL; chn++) {
            MediaSource *source =
                media_session->GetMediaSource((MediaChannelID)chn);
            if (source != nullptr) {
                rtp_conn_->SetClockRate((MediaChannelID)chn,
                                        source->GetClockRate());
                rtp_conn_->SetPayloadType((MediaChannelID)chn,
                                          source->GetPayload());
            }
        }

        std::string sdp = media_session->GetSdpMessage(
            GetSocketIp(this->GetSocket()), rtsp_server->GetVersion());
        if (sdp == "") {
            ret = BuildServerError_res(response, sizeof(response));
        } else {
            ret = BuildDescribe_res(response, sizeof(response), sdp.c_str());
        }
    }
    if (ret <= 0) {
        LOG_DEBUG("error:buildOption failed");
        return;
    }
    Send(response, ret);
}

void RtspConnect::HandleSetup() {
    char response[4096];
    int ret = 0;
    std::shared_ptr<MediaSession> media_session = nullptr;

    auto rtsp_server = server_.lock();
    if (rtsp_server) {
        media_session = rtsp_server->LookMediaSession(session_id_);
    }

    if (!rtsp_server || !media_session) {
        LOG_DEBUG("SetUp Erorr");
        ret = BuildServerError_res(response, sizeof(response));
        Send(response, strlen(response));
        return;
    }

    if (this->GetTransport() == TransportMode::RTP_OVER_UDP) {
        uint16_t per_rtp_port = GetRtpPort();
        uint16_t per_rtcp_port = GetRtcpPort();
        uint16_t session_id = rtp_conn_->GetRtpSessionId();
        auto ret =
            rtp_conn_->SetupRtpOverUdp(channelid_, per_rtp_port, per_rtcp_port);
        if (ret) {
            uint16_t ser_rtp_port = rtp_conn_->GetRtpPort(channelid_);
            uint16_t ser_rtcp_port = rtp_conn_->GetRtcpPort(channelid_);
            ret = BuildSetupUdp_res(response, sizeof(response), ser_rtp_port,
                                    ser_rtcp_port, session_id);
            if (ret <= 0) {
                LOG_DEBUG("error:BuildSetupUdp failed");
                return;
            }
            rtp_conn_->RtcpAsyncRead(channelid_);
            Send(response, strlen(response));
            return;
        } else {
            LOG_DEBUG("error:setup rtp over udp failed");
            // handleServererror
            ret = BuildServerError_res(response, sizeof(response));
            Send(response, strlen(response));
            return;
        }
    }

    uint16_t rtp_channel = GetRtpChannel();
    uint16_t rtcp_channel = GetRtcpChannel();
    uint16_t session_id = rtp_conn_->GetRtpSessionId();
    rtp_conn_->SetupRtpOverTcp(channelid_, rtp_channel, rtcp_channel);
    ret = BuildSetupTcp_res(response, sizeof(response), rtp_channel,
                            rtcp_channel, session_id);
    if (ret <= 0) {
        LOG_DEBUG("error:buildOption failed");
        return;
    }
    Send(response, strlen(response));
}

void RtspConnect::HandlePlay() {
    if (rtp_conn_ == nullptr) {
        return;
    }

    conn_state_ = ConnectionState::START_PLAY;
    rtp_conn_->Play();

    uint16_t session_id = rtp_conn_->GetRtpSessionId();
    char response[2048];

    int size = BuildPlay_res(response, sizeof(response), nullptr, session_id);
    Send(response, strlen(response));
}

void RtspConnect::HandleRtcp() {}

int RtspConnect::BuildOptions_res(char const *buf, size_t buf_size) {
    ::memset((void *)buf, 0, buf_size);
    snprintf((char *)buf, buf_size,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %u\r\n"
             "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n"
             "\r\n",
             this->GetCSeq());

    return (int)strlen(buf);
}

int RtspConnect::BuildDescribe_res(char const *res, size_t size,
                                   std::string sdp) {
    ::memset((void *)res, 0, size);

    snprintf((char *)res, size,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %u\r\n"
             "Content-Length: %d\r\n"
             "Content-Type: application/sdp\r\n"
             "\r\n"
             "%s",
             this->GetCSeq(), (int)strlen(sdp.c_str()), sdp.c_str());
    return (int)strlen(res);
}

/*在 TCP 承载 RTSP 的情况下，RTP 和 RTCP 数据与 RTSP 数据共享 TCP
数据通道，需要通过特定的标识来区分。
RTP 和 RTCP 数据会以$符号＋1 个字节的通道编号＋2 个字节的数据长度，共 4
个字节的前缀开始，而 RTSP 数据是没有前缀数据的。 例如，RTP
数据的包头可能是$00xxxx（其中$是标识符，00是偶数信道编号，表示数据信道，xxxx表示数据长度）；
RTCP 数据的包头可能是$01yyyy（其中$是标识符，01是奇数信道编号，即数据信道 0 加
1，表示控制信道，yyyy表示数据长度）*/
int RtspConnect::BuildSetupTcp_res(char const *res, size_t size,
                                   uint16_t rtp_chn, uint16_t rtcp_chn,
                                   uint32_t session_id) {
    ::memset((void *)res, 0, size);
    snprintf((char *)res, size,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %u\r\n"
             "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
             "Session: %u\r\n"
             "\r\n",
             this->GetCSeq(), rtp_chn, rtcp_chn, session_id);
    return (int)strlen(res);
}

int RtspConnect::BuildSetupUdp_res(char const *res, size_t size,
                                   uint16_t rtp_chn, uint16_t rtcp_chn,
                                   uint32_t session_id) {
    ::memset((void *)res, 0, size);
    snprintf((char *)res, size,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %u\r\n"
             "Transport: RTP/AVP;unicast;client_port=%u-%u;server_port"
             "=%u-%u\r\n"
             "Session: %u\r\n"
             "\r\n",
             this->GetCSeq(), this->GetRtpPort(), this->GetRtcpPort(), rtp_chn,
             rtcp_chn, session_id);
    return (int)strlen(res);
}

int RtspConnect::BuildPlay_res(char const *res, size_t size,
                               char const *rtpInfo, uint32_t session_id) {
    ::memset((void *)res, 0, size);
    snprintf((char *)res, size,
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %d\r\n"
             "Range: npt=0.00-\r\n"
             "Session: %u;timeout=60\r\n",
             this->GetCSeq(), session_id);

    if (rtpInfo != nullptr) {
        snprintf((char *)res + strlen(res), size - strlen(res), "%s\r\n",
                 rtpInfo);
    }
    snprintf((char *)res + strlen(res), size - strlen(res), "\r\n");
    return (int)strlen(res);
}

int RtspConnect::BuildNotFound_res(char const *res, int size) {
    memset((void *)res, 0, size);
    snprintf((char *)res, size,
             "RTSP/1.0 404 Stream Not Found\r\n"
             "CSeq: %u\r\n"
             "\r\n",
             this->GetCSeq());

    return (int)strlen(res);
}

int RtspConnect::BuildServerError_res(char const *res, int size) {
    memset((void *)res, 0, size);
    snprintf((char *)res, size,
             "RTSP/1.0 500 Internal Server Error\r\n"
             "CSeq: %u\r\n"
             "\r\n",
             this->GetCSeq());

    return (int)strlen(res);
}
