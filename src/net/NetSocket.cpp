#include "NetSocket.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "core/Logger.h"

NetSocket::NetSocket() = default;

NetSocket::NetSocket(int fd) : m_fd(fd) {}

NetSocket::~NetSocket()
{
    close();
}

NetSocket::NetSocket(NetSocket &&other) noexcept
    : m_fd(other.m_fd), m_recvBuffer(std::move(other.m_recvBuffer))
{
    other.m_fd = -1;
}

NetSocket &NetSocket::operator=(NetSocket &&other) noexcept
{
    if (this != &other) {
        close();
        m_fd = other.m_fd;
        m_recvBuffer = std::move(other.m_recvBuffer);
        other.m_fd = -1;
    }
    return *this;
}

bool NetSocket::initNetworking()
{
    // No-op on POSIX
    return true;
}

void NetSocket::shutdownNetworking()
{
    // No-op on POSIX
}

bool NetSocket::listen(uint16_t port, int backlog)
{
    close();

    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        WARN << "Failed to create socket:" << strerror(errno);
        return false;
    }

    // Allow address reuse
    int opt = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(m_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        WARN << "Failed to bind to port" << port << ":" << strerror(errno);
        close();
        return false;
    }

    if (::listen(m_fd, backlog) < 0) {
        WARN << "Failed to listen:" << strerror(errno);
        close();
        return false;
    }

    DBG << "Listening on port" << port;
    return true;
}

std::unique_ptr<NetSocket> NetSocket::accept()
{
    if (m_fd < 0) {
        return nullptr;
    }

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = ::accept(m_fd, reinterpret_cast<sockaddr *>(&clientAddr), &addrLen);

    if (clientFd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            WARN << "Accept failed:" << strerror(errno);
        }
        return nullptr;
    }

    DBG << "Accepted connection from" << inet_ntoa(clientAddr.sin_addr);
    return std::unique_ptr<NetSocket>(new NetSocket(clientFd));
}

bool NetSocket::connect(const std::string &host, uint16_t port)
{
    close();

    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        WARN << "Failed to create socket:" << strerror(errno);
        return false;
    }

    // Resolve hostname
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *result = nullptr;
    std::string portStr = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rc != 0) {
        WARN << "Failed to resolve host" << host << ":" << gai_strerror(rc);
        close();
        return false;
    }

    bool connected = false;
    for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        if (::connect(m_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            connected = true;
            break;
        }
    }
    ::freeaddrinfo(result);

    if (!connected) {
        WARN << "Failed to connect to" << host << ":" << port << "-" << strerror(errno);
        close();
        return false;
    }

    DBG << "Connected to" << host << ":" << port;
    return true;
}

void NetSocket::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_recvBuffer.clear();
}

bool NetSocket::setNonBlocking()
{
    if (m_fd < 0) return false;

    int flags = ::fcntl(m_fd, F_GETFL, 0);
    if (flags < 0) return false;

    return ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool NetSocket::setNoDelay()
{
    if (m_fd < 0) return false;

    int opt = 1;
    return ::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) == 0;
}

int NetSocket::sendRaw(const void *data, size_t length)
{
    if (m_fd < 0) return -1;

    ssize_t sent = ::send(m_fd, data, length, MSG_NOSIGNAL);
    return static_cast<int>(sent);
}

int NetSocket::recvRaw(void *buffer, size_t maxLength)
{
    if (m_fd < 0) return -1;

    ssize_t received = ::recv(m_fd, buffer, maxLength, 0);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }
        // Real error — close the socket
        close();
        return -1;
    }
    if (received == 0) {
        // Graceful disconnect by remote peer
        close();
        return 0;
    }
    return static_cast<int>(received);
}

bool NetSocket::sendMessage(const std::vector<uint8_t> &payload)
{
    if (m_fd < 0) return false;

    // 4-byte little-endian length prefix
    uint32_t len = static_cast<uint32_t>(payload.size());
    uint8_t header[4];
    header[0] = static_cast<uint8_t>(len & 0xFF);
    header[1] = static_cast<uint8_t>((len >> 8) & 0xFF);
    header[2] = static_cast<uint8_t>((len >> 16) & 0xFF);
    header[3] = static_cast<uint8_t>((len >> 24) & 0xFF);

    // Send header
    size_t totalSent = 0;
    while (totalSent < 4) {
        int sent = sendRaw(header + totalSent, 4 - totalSent);
        if (sent <= 0) return false;
        totalSent += sent;
    }

    // Send payload
    totalSent = 0;
    while (totalSent < payload.size()) {
        int sent = sendRaw(payload.data() + totalSent, payload.size() - totalSent);
        if (sent <= 0) return false;
        totalSent += sent;
    }

    return true;
}

bool NetSocket::recvMessage(std::vector<uint8_t> &out)
{
    // Read whatever is available into the buffer
    uint8_t tmp[4096];
    for (;;) {
        int n = recvRaw(tmp, sizeof(tmp));
        if (n > 0) {
            m_recvBuffer.insert(m_recvBuffer.end(), tmp, tmp + n);
        } else {
            // n == 0 means disconnect (socket already closed by recvRaw),
            // n < 0 means EAGAIN or error — either way, stop reading.
            break;
        }
    }

    // Need at least 4 bytes for the length header
    if (m_recvBuffer.size() < 4) {
        return false;
    }

    uint32_t msgLen = static_cast<uint32_t>(m_recvBuffer[0])
                    | (static_cast<uint32_t>(m_recvBuffer[1]) << 8)
                    | (static_cast<uint32_t>(m_recvBuffer[2]) << 16)
                    | (static_cast<uint32_t>(m_recvBuffer[3]) << 24);

    // Sanity check: reject absurdly large messages (16 MB)
    if (msgLen > 16 * 1024 * 1024) {
        WARN << "Received message with absurd length:" << msgLen;
        m_recvBuffer.clear();
        return false;
    }

    if (m_recvBuffer.size() < 4 + msgLen) {
        return false; // Incomplete message, wait for more data
    }

    // Extract the message
    out.assign(m_recvBuffer.begin() + 4, m_recvBuffer.begin() + 4 + msgLen);

    // Remove consumed bytes from the buffer
    m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + 4 + msgLen);

    return true;
}

bool NetSocket::isValid() const
{
    return m_fd >= 0;
}
