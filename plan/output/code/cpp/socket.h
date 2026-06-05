#pragma once
/**
 * F-PSU TCP socket layer — lightweight POSIX wrapper.
 * Asiacrypt 2026.
 *
 * Provides Sender/Receiver TCP communication for F-PSU protocol.
 * Each message is framed with a 4-byte big-endian length prefix.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace fpsu {
namespace net {

/// RAII TCP socket wrapper.
class TcpSocket {
public:
    TcpSocket();
    explicit TcpSocket(int fd);
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    /// Server: bind and listen on port. Returns listening socket.
    bool listen(int port);

    /// Server: accept one incoming connection. Returns new connected socket.
    TcpSocket accept();

    /// Client: connect to host:port.
    bool connect(const char* host, int port);

    /// Send a length-prefixed message. Returns false on error.
    bool send(const void* data, size_t len);
    bool send(const std::string& s) { return send(s.data(), s.size()); }
    bool send(const std::vector<uint8_t>& v) { return send(v.data(), v.size()); }

    /// Receive a length-prefixed message. Returns empty vector on error.
    std::vector<uint8_t> recv();

    /// Receive exactly n bytes. Returns empty vector on error.
    std::vector<uint8_t> recvAll(size_t n);

    /// Close the socket.
    void close();

    /// Is this socket valid?
    bool valid() const { return fd_ >= 0; }

    /// Underlying fd (for coproto / libOTe compatibility).
    int fd() const { return fd_; }

private:
    int fd_;

    bool recvAll(void* buf, size_t n);
    bool sendAll(const void* buf, size_t n);
};

} // namespace net
} // namespace fpsu
