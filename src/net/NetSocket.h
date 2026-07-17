#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>

/// RAII wrapper around a POSIX TCP socket.
/// Supports both blocking and non-blocking I/O with length-prefixed messaging.
class NetSocket
{
public:
    NetSocket();
    ~NetSocket();

    // Non-copyable
    NetSocket(const NetSocket &) = delete;
    NetSocket &operator=(const NetSocket &) = delete;

    // Movable
    NetSocket(NetSocket &&other) noexcept;
    NetSocket &operator=(NetSocket &&other) noexcept;

    /// Platform init (WSAStartup on Windows, no-op on POSIX)
    static bool initNetworking();
    /// Platform shutdown
    static void shutdownNetworking();

    /// Start listening on the given port. Returns true on success.
    bool listen(uint16_t port, int backlog = 8);

    /// Accept a pending connection. Returns a new socket, or nullptr if none available.
    std::unique_ptr<NetSocket> accept();

    /// Connect to a remote host. Returns true on success.
    bool connect(const std::string &host, uint16_t port);

    /// Close the socket.
    void close();

    /// Set the socket to non-blocking mode.
    bool setNonBlocking();

    /// Set TCP_NODELAY (disable Nagle's algorithm).
    bool setNoDelay();

    /// Send raw bytes. Returns number of bytes sent, or -1 on error.
    int sendRaw(const void *data, size_t length);

    /// Receive raw bytes. Returns number of bytes received, 0 on disconnect, -1 on EAGAIN/error.
    int recvRaw(void *buffer, size_t maxLength);

    /// Send a length-prefixed message (4-byte LE length header + payload).
    bool sendMessage(const std::vector<uint8_t> &payload);

    /// Try to receive a complete length-prefixed message.
    /// Returns true if a full message was received and written to `out`.
    /// Returns false if no complete message is available yet (partial read buffered).
    bool recvMessage(std::vector<uint8_t> &out);

    /// True if the socket file descriptor is valid.
    bool isValid() const;

    /// Return the raw file descriptor (for polling).
    int fd() const { return m_fd; }

private:
    explicit NetSocket(int fd);

    int m_fd = -1;
    std::vector<uint8_t> m_recvBuffer; ///< Accumulates partial reads
};
