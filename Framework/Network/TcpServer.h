#pragma once

#include <memory>

// io_context的封装
class Service;
class ConnectionImpl;
class Acceptor;
class TcpServer : public std::enable_shared_from_this<TcpServer> {};