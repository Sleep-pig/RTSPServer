#include "net/H264File.hpp"
#include "net/H264Source.hpp"
#include "net/media.hpp"
#include "net/RtspServer.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <Log/logger.hpp>
#include <memory>
#include <thread>

void SendFrameThread(RtspServer *rtsp_server, MediaSessionId session_id,
                     H264File *h264_file);

int main() {
    try {
        H264File h264_file;
        if (h264_file.Open(
                "/home/jie/workspace/cpp/RTSP/src/net/core/test.h264") ==
            false) {
            LOG_DEBUG("打开文件失败");
            return 0;
        }

        std::string suffix = "live";
        std::string ip = "127.0.0.1";
        std::string port = "8554";
        std::string rtsp_url = "rtsp://" + ip + ":" + port + "/" + suffix;

        boost::asio::io_context ioc{
            1}; // 创建一个io_context对象，该对象内部包含一个单独的线程来处理异步I/O操作
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&ioc](boost::system::error_code const &error, int signal_number) {
                if (error) {
                    return;
                }
                ioc.stop();
            });

        std::shared_ptr<RtspServer> server =
            std::make_shared<RtspServer>(ioc, 8554);
        server->Start();

        auto session = MediaSession::GetInstance("live");
        session->AddSource(channel0, H264Source::GetInstance().get());
        // session->StartMulticast();
        session->AddNotifyConnectedCallback([](MediaSessionId sessionId,
                                               std::string peer_ip,
                                               uint16_t peer_port) {
            printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(),
                   peer_port);
        });

        session->AddNotifyDisconnectedCallback([](MediaSessionId sessionId,
                                                  std::string peer_ip,
                                                  uint16_t peer_port) {
            printf("RTSP client disconnect, ip=%s, port=%hu \n",
                   peer_ip.c_str(), peer_port);
        });

        MediaSessionId session_id = server->AddSession(session.get());

        std::thread t1(SendFrameThread, server.get(), session_id, &h264_file);
        t1.detach();

        std::cout << "Play URL: " << rtsp_url << std::endl;

        ioc.run();

        return 0;

    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

void SendFrameThread(RtspServer *rtsp_server, MediaSessionId session_id,
                     H264File *h264_file) {
    int buf_size = 2'000'000;
    std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

    while (1) {
        bool end_of_frame = false;
        int frame_size = h264_file->ReadFrame((char *)frame_buf.get(), buf_size,
                                              &end_of_frame);
        if (frame_size > 0) {
            AVFrame videoFrame = {0};
            videoFrame.type = 0;
            videoFrame.size = frame_size;
            videoFrame.timestamp = H264Source::GetTimeStamp();
            videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
            memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
            rtsp_server->PushFrame(session_id, channel0, videoFrame);
        } else {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    };
}
