#ifndef CHAT_HPP
#define CHAT_HPP

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <random>
#include <cstring>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib,"ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    using SOCKET = int;
    constexpr SOCKET INVALID_SOCKET = -1;
    inline void closesocket(SOCKET s) { ::close(s); }
#endif

inline constexpr const char* BROADCAST_ADDR = "255.255.255.255";
inline constexpr int PORT         = 50000;
inline constexpr int BUFFER_SIZE  = 16000;

std::string generate_client_id();

class ChatBackend {
public:
    using MessageCallback = std::function<void(const std::string& fromId,
                                               const std::string& text)>;

    ChatBackend();
    ~ChatBackend();

    bool start(MessageCallback cb);

    void stop();

    void enqueueMessage(const std::string& text);

    std::string clientId() const { return clientId_; }

private:
    void recvLoop();
    void sendLoop();

    SOCKET sockfd_{INVALID_SOCKET};
    std::atomic<bool> running_{false};

    std::thread recvThread_;
    std::thread sendThread_;

    std::mutex outMutex_;
    std::condition_variable outCv_;
    std::queue<std::string> outgoing_;

    MessageCallback callback_;
    std::string clientId_;
};

#endif // CHAT_HPP
