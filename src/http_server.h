//
// Created by dungnd on 07/06/2022.
//

#ifndef BASIC_HTTP_SERVER_HTTP_SERVER_H
#define BASIC_HTTP_SERVER_HTTP_SERVER_H

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>
#include <functional>
#include <thread>
#include <map>

#include "http_message.h"

namespace basic_http_server {
    constexpr size_t kMaxBufferSize = 4096;

    struct Event {
        Event() : fd(0), length(0), cursor(0), buffer() {}
        int fd;
        size_t length;
        size_t cursor;
        char buffer[kMaxBufferSize];
    };

    // Request handle have a HTTP request as input and return a HTTP response
    using HttpRequestHandler_t = std::function<HttpResponse(const HttpRequest&)>;

    /**
     * Class for Http Server operation
     * Consist of:
     * 1 main thread: create socket
     * 1 listener thread: listen, distribute request from accepted new connections
     * N worker thread: process Http request by event
     */
    class HttpServer {
    public:
        explicit HttpServer(const std::string& host, std::uint16_t port);
        ~HttpServer() = default;

        HttpServer() = default;
        HttpServer(HttpServer&&) = default;
        HttpServer& operator=(HttpServer&&) = default;
        /**
         * Start Http Server
         */
        void Start();
        void Stop();
        void RegisterHttpRequestHandler(const std::string& path, HttpMethod method, const HttpRequestHandler_t callback);
        void RegisterHttpRequestHandler(const Uri uri, HttpMethod method, const HttpRequestHandler_t callback);

        std::string host() const { return host_; }
        std::uint16_t port() const { return port_; }
        bool running() const { return running_; }
    private:

    private:
        static constexpr int kBacklogSize = 1000;
        static constexpr int kMaxConnections = 10000;
        static constexpr int kMaxEvents = 10000;
        static constexpr int kThreadPoolSize = 10;

        std::string host_;
        std::uint16_t port_;
        int sock_fd_;
        bool running_;
        std::thread listener_;
        std::thread workers_[kThreadPoolSize];
        int worker_epoll_fd_[kThreadPoolSize];
        epoll_event worker_events_[kThreadPoolSize][kMaxEvents];
        std::map<Uri, std::map<HttpMethod, HttpRequestHandler_t>> request_handlers_;

        void InitSocket();
        void InitEpoll();
        void Listen();
        void ProcessEvent(int worker_id);
        void HandleEpollEvent(int epoll_fd, Event *event, std::uint32_t events);
        void HandleHttpData(const Event& request_event, Event* response_event);
        HttpResponse HandleHttpRequest(const HttpRequest& request);

        void ControlEpollEvent(int epoll_fd, int op, int fd, std::uint32_t events = 0, void *data = nullptr);
    };
}

#endif //BASIC_HTTP_SERVER_HTTP_SERVER_H
