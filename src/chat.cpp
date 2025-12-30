#include "ClipTransfer/chat.hpp"
#include <sys/types.h>
#include <limits>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace {

constexpr const char* PROTO_PREFIX = "CT2";

// Taille cible d'un datagramme UDP (évite fragmentation IP -> pertes fréquentes en broadcast)
constexpr size_t MAX_DATAGRAM_BYTES = 1200;

// Taille brute par chunk avant base64 (base64 ~ +33%)
constexpr size_t RAW_CHUNK_BYTES = 800;

constexpr int MAX_PARTS = 30000; // Permet jusqu'à ~24 Mo (30000 * 800 octets)
constexpr auto REASSEMBLY_TTL = std::chrono::seconds(30);

static std::string generate_message_id() {
    static const char chars[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(0, 15);

    std::string id;
    id.reserve(12);
    for (int i = 0; i < 12; ++i) {
        id.push_back(chars[d(gen)]);
    }
    return id;
}

static const char* b64_table() {
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

static std::string base64_encode(std::string_view data) {
    const char* tbl = b64_table();
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= data.size()) {
        const unsigned char b0 = static_cast<unsigned char>(data[i]);
        const unsigned char b1 = static_cast<unsigned char>(data[i + 1]);
        const unsigned char b2 = static_cast<unsigned char>(data[i + 2]);
        i += 3;

        out.push_back(tbl[(b0 >> 2) & 0x3F]);
        out.push_back(tbl[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        out.push_back(tbl[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)]);
        out.push_back(tbl[b2 & 0x3F]);
    }

    const size_t rem = data.size() - i;
    if (rem == 1) {
        const unsigned char b0 = static_cast<unsigned char>(data[i]);
        out.push_back(tbl[(b0 >> 2) & 0x3F]);
        out.push_back(tbl[(b0 & 0x03) << 4]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const unsigned char b0 = static_cast<unsigned char>(data[i]);
        const unsigned char b1 = static_cast<unsigned char>(data[i + 1]);
        out.push_back(tbl[(b0 >> 2) & 0x3F]);
        out.push_back(tbl[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        out.push_back(tbl[(b1 & 0x0F) << 2]);
        out.push_back('=');
    }

    return out;
}

static int b64_index(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static bool base64_decode(const std::string& in, std::string& out) {
    out.clear();
    if (in.empty()) return true;
    if ((in.size() % 4) != 0) return false;

    out.reserve((in.size() / 4) * 3);

    for (size_t i = 0; i < in.size(); i += 4) {
        const unsigned char c0 = static_cast<unsigned char>(in[i]);
        const unsigned char c1 = static_cast<unsigned char>(in[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(in[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(in[i + 3]);

        const int v0 = b64_index(c0);
        const int v1 = b64_index(c1);
        const bool pad2 = (c2 == '=');
        const bool pad3 = (c3 == '=');
        const int v2 = pad2 ? 0 : b64_index(c2);
        const int v3 = pad3 ? 0 : b64_index(c3);
        if (v0 < 0 || v1 < 0 || (!pad2 && v2 < 0) || (!pad3 && v3 < 0)) return false;

        const unsigned int triple = (static_cast<unsigned int>(v0) << 18)
                                  | (static_cast<unsigned int>(v1) << 12)
                                  | (static_cast<unsigned int>(v2) << 6)
                                  | static_cast<unsigned int>(v3);

        out.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (!pad2) out.push_back(static_cast<char>((triple >> 8) & 0xFF));
        if (!pad3) out.push_back(static_cast<char>(triple & 0xFF));
    }

    return true;
}

static bool parse_int(const std::string& s, int& out) {
    if (s.empty()) return false;
    long long v = 0;
    for (char ch : s) {
        if (ch < '0' || ch > '9') return false;
        v = v * 10 + (ch - '0');
        if (v > std::numeric_limits<int>::max()) return false;
    }
    out = static_cast<int>(v);
    return true;
}

struct PendingMessage {
    std::string senderId;
    std::string senderName;
    int totalParts = 0;
    int receivedParts = 0;
    size_t totalBytes = 0;
    std::vector<std::string> parts;
    std::vector<uint8_t> have;
    std::chrono::steady_clock::time_point lastUpdate;
};

static bool split_field(const std::string& s, size_t& pos, std::string& outField) {
    if (pos > s.size()) return false;
    const size_t next = s.find('|', pos);
    if (next == std::string::npos) {
        outField = s.substr(pos);
        pos = s.size();
        return true;
    }
    outField = s.substr(pos, next - pos);
    pos = next + 1;
    return true;
}

}

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

void ChatBackend::clearHistory() {
    // Purge immédiate de la file d'envoi
    {
        std::lock_guard<std::mutex> lock(outMutex_);
        std::queue<std::string> empty;
        outgoing_.swap(empty);
    }

    // Demande au thread de réception de purger ses états (réassemblage)
    clearEpoch_.fetch_add(1, std::memory_order_relaxed);

    // Réveille le thread d'envoi si besoin
    outCv_.notify_all();
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

            // Protocole v2 : CT2|<senderId>|<pseudo>|<msgId>|<seq>|<total>|<base64(chunk)>
            // On découpe en petits datagrammes pour éviter la fragmentation IP (très mauvaise en broadcast)
            const std::string msgId = generate_message_id();

            const size_t totalLen = text.size();
            const int totalParts = static_cast<int>((totalLen + RAW_CHUNK_BYTES - 1) / RAW_CHUNK_BYTES);
            if (totalParts <= 0 || totalParts > MAX_PARTS) {
                std::cerr << "message too large (parts)\n";
                lock.lock();
                continue;
            }

            for (int seq = 0; seq < totalParts; ++seq) {
                const size_t start = static_cast<size_t>(seq) * RAW_CHUNK_BYTES;
                const size_t len = std::min(RAW_CHUNK_BYTES, totalLen - start);
                std::string_view chunk(text.data() + start, len);
                std::string b64 = base64_encode(chunk);

                std::string packet;
                packet.reserve(64 + b64.size());
                packet.append(PROTO_PREFIX);
                packet.push_back('|');
                packet.append(clientId_);
                packet.push_back('|');
                packet.append(nickname_);
                packet.push_back('|');
                packet.append(msgId);
                packet.push_back('|');
                packet.append(std::to_string(seq));
                packet.push_back('|');
                packet.append(std::to_string(totalParts));
                packet.push_back('|');
                packet.append(b64);

                if (packet.size() > MAX_DATAGRAM_BYTES) {
                    // Garde-fou : si on dépasse, on refuse (évite fragmentation)
                    std::cerr << "packet too large (would fragment)\n";
                    break;
                }

                if (packet.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
                    std::cerr << "packet too large for sendto()\n";
                    break;
                }

                const int lenInt = static_cast<int>(packet.size());
                ssize_t sent = ::sendto(sockfd_, packet.data(), lenInt, 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

                if (sent < 0) {
                    std::cerr << "sendto() failed\n";
                }
            }

            lock.lock();
        }
    }
}

void ChatBackend::recvLoop() {
    char buffer[BUFFER_SIZE];

    std::unordered_map<std::string, PendingMessage> pending;
    std::uint64_t localClearEpoch = clearEpoch_.load(std::memory_order_relaxed);

    while (running_) {
        const std::uint64_t currentEpoch = clearEpoch_.load(std::memory_order_relaxed);
        if (currentEpoch != localClearEpoch) {
            pending.clear();
            localClearEpoch = currentEpoch;
        }

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

        // Nettoyage des messages incomplets trop vieux
        {
            const auto now = std::chrono::steady_clock::now();
            for (auto it = pending.begin(); it != pending.end();) {
                if ((now - it->second.lastUpdate) > REASSEMBLY_TTL) {
                    it = pending.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Nouveau format : CT2|<senderId>|<pseudo>|<msgId>|<seq>|<total>|<base64(chunk)>
        // Ancien format : <CLIENT_ID>|<PSEUDO>|<texte>
        if (msg.rfind(PROTO_PREFIX, 0) == 0) {
            size_t pos = 0;
            std::string fPrefix, fSenderId, fName, fMsgId, fSeq, fTotal, fB64;
            if (!split_field(msg, pos, fPrefix)) continue;
            if (!split_field(msg, pos, fSenderId)) continue;
            if (!split_field(msg, pos, fName)) continue;
            if (!split_field(msg, pos, fMsgId)) continue;
            if (!split_field(msg, pos, fSeq)) continue;
            if (!split_field(msg, pos, fTotal)) continue;
            // dernier champ = reste
            fB64 = (pos <= msg.size()) ? msg.substr(pos) : std::string{};

            if (fSenderId.empty() || fName.empty() || fMsgId.empty() || fSeq.empty() || fTotal.empty()) continue;
            if (fSenderId == clientId_) continue;

            int seq = 0;
            int total = 0;
            if (!parse_int(fSeq, seq) || !parse_int(fTotal, total)) continue;
            if (total <= 0 || total > MAX_PARTS) continue;
            if (seq < 0 || seq >= total) continue;

            std::string chunk;
            if (!base64_decode(fB64, chunk)) continue;

            // Clé = senderId:msgId
            std::string key;
            key.reserve(fSenderId.size() + 1 + fMsgId.size());
            key.append(fSenderId);
            key.push_back(':');
            key.append(fMsgId);

            auto now = std::chrono::steady_clock::now();
            auto it = pending.find(key);
            if (it == pending.end()) {
                PendingMessage pm;
                pm.senderId = fSenderId;
                pm.senderName = fName;
                pm.totalParts = total;
                pm.receivedParts = 0;
                pm.totalBytes = 0;
                pm.parts.resize(static_cast<size_t>(total));
                pm.have.assign(static_cast<size_t>(total), 0);
                pm.lastUpdate = now;
                it = pending.emplace(std::move(key), std::move(pm)).first;
            }

            PendingMessage& pm = it->second;
            pm.lastUpdate = now;

            if (pm.parts.empty()) {
                pm.totalParts = total; // Assurez-vous que la variable s'appelle 'total' ici

                try {
                    pm.parts.resize(static_cast<size_t>(total));
                    pm.have.resize(static_cast<size_t>(total), 0);
                } catch (const std::bad_alloc&) {
                    pending.erase(it);
                    continue;
                }
            }

            if (pm.totalParts != total) {
                pending.erase(it);
                continue;
            }

            if (!pm.have[static_cast<size_t>(seq)]) {
                pm.parts[static_cast<size_t>(seq)] = std::move(chunk);
                pm.have[static_cast<size_t>(seq)] = 1;
                pm.receivedParts += 1;
                pm.totalBytes += pm.parts[static_cast<size_t>(seq)].size();
            }

            if (pm.receivedParts == pm.totalParts) {
                std::string full;
                full.reserve(pm.totalBytes);
                for (const auto& p : pm.parts) {
                    full.append(p);
                }

                if (callback_ && !full.empty()) {
                    callback_(pm.senderName, full);
                }
                pending.erase(it);
            }

            continue;
        }

        // Ancien format : "<CLIENT_ID>|<PSEUDO>|<texte>"
        auto pos1 = msg.find('|');
        if (pos1 == std::string::npos) continue;

        auto pos2 = msg.find('|', pos1 + 1);
        if (pos2 == std::string::npos) continue;

        std::string senderId = msg.substr(0, pos1);
        if (senderId == clientId_) continue;

        std::string senderName = msg.substr(pos1 + 1, pos2 - (pos1 + 1));
        std::string text = msg.substr(pos2 + 1);

        if (senderName.empty() || text.empty()) continue;

        if (callback_) {
            callback_(senderName, text);
        }
    }
}
