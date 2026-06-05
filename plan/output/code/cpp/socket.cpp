/**
 * F-PSU TCP socket layer — POSIX implementation.
 * Asiacrypt 2026.
 */
#include "socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace fpsu {
namespace net {

// ============================================================
// TcpSocket
// ============================================================

TcpSocket::TcpSocket() : fd_(-1) {}

TcpSocket::TcpSocket(int fd) : fd_(fd) {}

TcpSocket::~TcpSocket() { close(); }

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void TcpSocket::close() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

// ---- Server ----

bool TcpSocket::listen(int port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "TcpSocket::listen: socket() failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "TcpSocket::listen: bind() failed: "
                  << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (::listen(fd_, 1) < 0) {
        std::cerr << "TcpSocket::listen: listen() failed: "
                  << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    std::cout << "  Server listening on port " << port << std::endl;
    return true;
}

TcpSocket TcpSocket::accept() {
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = ::accept(fd_, (struct sockaddr*)&clientAddr, &addrLen);
    if (clientFd < 0) {
        std::cerr << "TcpSocket::accept: accept() failed: "
                  << strerror(errno) << std::endl;
        return TcpSocket();
    }
    std::cout << "  Accepted connection from "
              << inet_ntoa(clientAddr.sin_addr) << std::endl;
    return TcpSocket(clientFd);
}

// ---- Client ----

bool TcpSocket::connect(const char* host, int port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "TcpSocket::connect: socket() failed: "
                  << strerror(errno) << std::endl;
        return false;
    }

    struct hostent* server = gethostbyname(host);
    if (!server) {
        std::cerr << "TcpSocket::connect: gethostbyname(" << host
                  << ") failed" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(port);

    if (::connect(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "TcpSocket::connect: connect() failed: "
                  << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    std::cout << "  Connected to " << host << ":" << port << std::endl;
    return true;
}

// ---- I/O helpers ----

bool TcpSocket::sendAll(const void* buf, size_t n) {
    const char* p = (const char*)buf;
    size_t sent = 0;
    while (sent < n) {
        ssize_t r = ::send(fd_, p + sent, n - sent, 0);
        if (r <= 0)
            return false;
        sent += r;
    }
    return true;
}

bool TcpSocket::recvAll(void* buf, size_t n) {
    char* p = (char*)buf;
    size_t rcvd = 0;
    while (rcvd < n) {
        ssize_t r = ::recv(fd_, p + rcvd, n - rcvd, 0);
        if (r <= 0)
            return false;
        rcvd += r;
    }
    return true;
}

// ---- Framed messages (4-byte BE length prefix) ----

bool TcpSocket::send(const void* data, size_t len) {
    if (len > 0xFFFFFFFFULL) {
        std::cerr << "TcpSocket::send: message too large (" << len << " bytes)"
                  << std::endl;
        return false;
    }

    uint32_t netLen = htonl((uint32_t)len);
    if (!sendAll(&netLen, 4))
        return false;
    if (len > 0 && !sendAll(data, len))
        return false;
    return true;
}

std::vector<uint8_t> TcpSocket::recv() {
    uint32_t netLen;
    if (!recvAll(&netLen, 4))
        return {};
    uint32_t len = ntohl(netLen);

    if (len == 0)
        return {};

    std::vector<uint8_t> buf(len);
    if (!recvAll(buf.data(), len))
        return {};
    return buf;
}

std::vector<uint8_t> TcpSocket::recvAll(size_t n) {
    std::vector<uint8_t> buf(n);
    if (!recvAll(buf.data(), n))
        return {};
    return buf;
}

} // namespace net
} // namespace fpsu
