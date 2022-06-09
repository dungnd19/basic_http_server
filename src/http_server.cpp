//
// Created by dungnd on 07/06/2022.
//

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sstream>

#include "http_server.h"

namespace basic_http_server{

    HttpServer::HttpServer(const std::string &host, std::uint16_t port) :
        host_(host), port_(port), sock_fd_(0), running_(false), worker_epoll_fd_(){
        InitSocket();
    }

    void HttpServer::Start() {
        int opt = 1;
        sockaddr_in server_address;

        if (setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("Failed to set socket options");
        }

        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        inet_pton(AF_INET, host_.c_str(), &(server_address.sin_addr.s_addr));
        server_address.sin_port = htons(port_);
        if (bind(sock_fd_, (sockaddr *)&server_address, sizeof(server_address)) < 0) {
            throw std::runtime_error("Failed to bind to socket");
        }

        if (listen(sock_fd_, kBacklogSize) < 0) {
            std::ostringstream msg;
            msg << "Failed to listen on port " << port_;
            throw std::runtime_error(msg.str());
        }

        InitEpoll();
        running_ = true;
        listener_ = std::thread(&HttpServer::Listen, this);
        for (int i = 0; i < kThreadPoolSize; i++) {
            workers_[i] = std::thread(&HttpServer::ProcessEvent, this, i);
        }
    }

    void HttpServer::Stop() {
        running_ = false;
        // join all threads
        listener_.join();
        for (int i = 0; i < kThreadPoolSize; i++) {
            workers_[i].join();
        }
        // close epoll_fd
        for (int i = 0; i < kThreadPoolSize; i++) {
            close(worker_epoll_fd_[i]);
        }
        // close socket
        close(sock_fd_);
    }

    void HttpServer::RegisterHttpRequestHandler(const std::string &path, HttpMethod method,
                                                const HttpRequestHandler_t callback) {
        Uri uri(path);
        request_handlers_[uri].insert(std::make_pair(method, std::move(callback)));
    }

    void HttpServer::RegisterHttpRequestHandler(const Uri uri, HttpMethod method, const HttpRequestHandler_t callback) {
        request_handlers_[uri].insert(std::make_pair(method, std::move(callback)));
    }

    void HttpServer::InitSocket() {
        if ((sock_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
            throw std::runtime_error("Failed to create a TCP socket");
        }
    }

    void HttpServer::InitEpoll() {
        for (int i = 0; i < kThreadPoolSize; i++) {
            if ((worker_epoll_fd_[i] = epoll_create1(0)) < 0) {
                throw std::runtime_error("Failed to create epoll file descriptor for worker");
            }
        }
    }

    void HttpServer::Listen() {
        Event *client_data;
        sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int client_fd;
        int current_worker = 0;

        // accept new connections and distribute tasks to worker threads
        while (running_) {
            client_fd = accept4(sock_fd_, (sockaddr *)&client_address, &client_len, SOCK_NONBLOCK);
            if (client_fd < 0) continue;

            client_data = new Event();
            client_data->fd = client_fd;
            ControlEpollEvent(worker_epoll_fd_[current_worker], EPOLL_CTL_ADD, client_fd, EPOLLIN, client_data);
            current_worker++;
            if (current_worker == HttpServer::kThreadPoolSize) current_worker = 0;
        }
    }

    void HttpServer::ProcessEvent(int worker_id) {
        Event *data;
        int epoll_fd = worker_epoll_fd_[worker_id];
        while (running_) {
            int event_nums = epoll_wait(worker_epoll_fd_[worker_id], worker_events_[worker_id], HttpServer::kMaxEvents, 0);
            if (event_nums < 0) {
                continue;
            }

            for (int i = 0; i < event_nums; i++) {
                const epoll_event& current_event = worker_events_[worker_id][i];
                data = reinterpret_cast<Event *>(current_event.data.ptr);
                if ((current_event.events & EPOLLHUP) || (current_event.events & EPOLLERR)) {
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                } else if ((current_event.events == EPOLLIN) || (current_event.events == EPOLLOUT)) {
                    HandleEpollEvent(epoll_fd, data, current_event.events);
                } else {  // something unexpected, delete event
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_DEL, data->fd);
                    close(data->fd);
                    delete data;
                }
            }
        }
    }

    void HttpServer::HandleEpollEvent(int epoll_fd, Event *event, std::uint32_t events) {
        int fd = event->fd;
        Event *request, *response;

        if (events == EPOLLIN) {
            // EPOLLIN event, recv request, analyze and create response
            request = event;
            ssize_t byte_count = recv(fd, request->buffer, kMaxBufferSize, 0);
            if (byte_count > 0) {           // we have fully received the message
                response = new Event();
                response->fd = fd;
                HandleHttpData(*request, response);
                // add EPOLLOUT event
                ControlEpollEvent(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                delete request;
            } else if (byte_count == 0) {   // client has closed connection
                ControlEpollEvent(epoll_fd, EPOLL_CTL_DEL, fd);
                close(fd);
                delete request;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {  // retry
                    request->fd = fd;
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                } else {                                        // other error
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_DEL, fd);
                    close(fd);
                    delete request;
                }
            }
        } else {
            // EPOLLOUT event, send response
            response = event;
            ssize_t byte_count = send(fd, response->buffer + response->cursor, response->length, 0);
            if (byte_count >= 0) {
                if (byte_count < response->length) {  // there are still bytes to write
                    response->cursor += byte_count;
                    response->length -= byte_count;
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLOUT, response);
                } else {                              // we have written the complete message
                    request = new Event();
                    request->fd = fd;
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_MOD, fd, EPOLLIN, request);
                    delete response;
                }
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {  // retry
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_ADD, fd, EPOLLOUT, response);
                } else {                                        // other error
                    ControlEpollEvent(epoll_fd, EPOLL_CTL_DEL, fd);
                    close(fd);
                    delete response;
                }
            }
        }
    }

    void HttpServer::HandleHttpData(const Event& request_event, Event* response_event) {
        std::string request_string(request_event.buffer), response_string;
        HttpRequest http_request;
        HttpResponse http_response;

        try {
            http_request = string_to_request(request_string);
            http_response = HandleHttpRequest(http_request);
        } catch (const std::invalid_argument& e) {
            http_response = HttpResponse(HttpStatusCode::BadRequest);
            http_response.SetContent(e.what());
        } catch (const std::logic_error& e) {
            http_response = HttpResponse(HttpStatusCode::HttpVersionNotSupported);
            http_response.SetContent(e.what());
        } catch (const std::exception& e) {
            http_response = HttpResponse(HttpStatusCode::InternalServerError);
            http_response.SetContent(e.what());
        }

        // Set response to write to client
        response_string = to_string(http_response, http_request.method() != HttpMethod::HEAD);
        strncpy(response_event->buffer, response_string.c_str(), kMaxBufferSize);
        response_event->length = response_string.length();
    }

    HttpResponse HttpServer::HandleHttpRequest(const HttpRequest &request) {
        auto it = request_handlers_.find(request.uri());
        // static uri
        // if (request.uri())

        if (it == request_handlers_.end()) {    // unregistered uri
            return HttpResponse(HttpStatusCode::NotFound);
        }
        auto callback_it = it->second.find(request.method());
        if (callback_it == it->second.end()) {  // unregistered handler
            return HttpResponse(HttpStatusCode::MethodNotAllowed);
        }
        return callback_it->second(request);    // call handler to process the request
    }

    void HttpServer::ControlEpollEvent(int epoll_fd, int op, int fd, std::uint32_t events, void *data) {
        if (op == EPOLL_CTL_DEL) {
            if (epoll_ctl(epoll_fd, op, fd, nullptr) < 0) {
                throw std::runtime_error("Failed to remove file descriptor");
            }
        } else {
            epoll_event ev;
            ev.events = events;
            ev.data.ptr = data;
            if (epoll_ctl(epoll_fd, op, fd, &ev) < 0) {
                throw std::runtime_error("Failed to add file descriptor");
            }
        }
    }
}

