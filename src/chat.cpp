#include "ClipTransfer/chat.hpp"
#include <sys/types.h>

std::string generate_client_id() {
    static const char chars[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(0, 15);

    std::string id;
    id.reserve(16);
    for (int i = 0; i < 16; ++i) {
        id.push_back(chars[d(gen)]);
    }
    return id;
}

ChatBackend::ChatBackend()
    : clientId_(generate_client_id()) {}

ChatBackend::~ChatBackend() {
    stop();
}

bool ChatBackend::start(MessageCallback cb) {
    if (running_) return true;
    callback_ = std::move(cb);

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
#endif

    sockfd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_ == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // Autoriser le broadcast
    int yes = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&yes),
                   sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_BROADCAST) failed\n";
        closesocket(sockfd_);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    // Réutilisation d'adresse
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&yes),
                   sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed\n";
        // non fatal, on continue
    }

    // Timeout de réception pour ne pas bloquer à l'infini
#ifdef _WIN32
    {
        int timeoutMs = 200;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeoutMs),
                   sizeof(timeoutMs));
    }
#else
    {
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000; // 200 ms
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv));
    }
#endif

    // Bind sur tous les interfaces
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd_, reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0) {
        std::cerr << "bind() failed\n";
        closesocket(sockfd_);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    running_ = true;

    recvThread_ = std::thread(&ChatBackend::recvLoop, this);
    sendThread_ = std::thread(&ChatBackend::sendLoop, this);

    return true;
}

void ChatBackend::stop() {
    if (!running_) return;
    running_ = false;
    outCv_.notify_all();

    if (sendThread_.joinable()) sendThread_.join();
    if (recvThread_.joinable()) recvThread_.join();

    if (sockfd_ != INVALID_SOCKET) {
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

void ChatBackend::enqueueMessage(const std::string& text) {
    if (!running_) return;
    {
        std::lock_guard<std::mutex> lock(outMutex_);
        outgoing_.push(text);
    }
    outCv_.notify_one();
}

void ChatBackend::sendLoop() {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(PORT);
    dest.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);

    while (true) {
        std::unique_lock<std::mutex> lock(outMutex_);
        outCv_.wait(lock, [this]() {
            return !running_ || !outgoing_.empty();
        });

        if (!running_ && outgoing_.empty()) {
            break;
        }

        while (!outgoing_.empty()) {
            std::string text = std::move(outgoing_.front());
            outgoing_.pop();
            lock.unlock();

            std::string packet = clientId_ + "|" + nickname_ + "|" + text;
            size_t len = packet.size();

            ssize_t sent = ::sendto(sockfd_,
                                packet.data(),
                                len,
                                0,
                                reinterpret_cast<sockaddr*>(&dest),
                                sizeof(dest));
            if (sent < 0) {
                // on log juste, on ne plante pas
                std::cerr << "sendto() failed\n";
            }

            lock.lock();
        }
    }
}

void ChatBackend::recvLoop() {
    char buffer[BUFFER_SIZE];

    while (running_) {
        sockaddr_in src{};
#ifdef _WIN32
        int srclen = sizeof(src);
#else
        socklen_t srclen = sizeof(src);
#endif
        ssize_t received = ::recvfrom(sockfd_,
                                  buffer,
                                  BUFFER_SIZE,
                                  0,
                                  reinterpret_cast<sockaddr*>(&src),
                                  &srclen);
        if (received <= 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                continue;
            }
#else
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
#endif
            if (!running_) break;
            // autre erreur : on log et on continue
            std::cerr << "recvfrom() failed\n";
            continue;
        }

        std::string msg(buffer, buffer + received);

        // Format : "<CLIENT_ID>|<PSEUDO>|<texte>"
        auto pos1 = msg.find('|');
        if (pos1 == std::string::npos) continue;

        auto pos2 = msg.find('|', pos1 + 1);
        if (pos2 == std::string::npos) continue; // pas de pseudo => paquet invalide

        std::string senderId = msg.substr(0, pos1);
        if (senderId == clientId_) continue; // ignorer nos propres messages

        std::string senderName = msg.substr(pos1 + 1, pos2 - (pos1 + 1));
        std::string text = msg.substr(pos2 + 1);

        if (senderName.empty() || text.empty()) continue; // optionnel: refuser vide

        if (callback_) {
            callback_(senderName, text);
        }
    }
}
