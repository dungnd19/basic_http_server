#include <iostream>

#include "http_server.h"

using namespace basic_http_server;

int main(void) {
    std::string host = "0.0.0.0";
    int port = 8080;
    HttpServer server(host, port);

    // Register a few endpoints for demo and benchmarking
    auto say_hello = [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response(HttpStatusCode::Ok);
        response.SetHeader("Content-Type", "text/html");
        std::string content;

        response.SetContent(content);
        return response;
    };
    auto send_html = [](const HttpRequest& request) -> HttpResponse {
        HttpResponse response(HttpStatusCode::Ok);
        std::string content;
        content += "<!doctype html>\n";
        content += "<html>\n<body>\n\n";
        content += "<h1>Hello, world in an Html page</h1>\n";
        content += "<p>A Paragraph</p>\n\n";
        content += "</body>\n</html>\n";

        response.SetHeader("Content-Type", "text/html");
        response.SetContent(content);
        return response;
    };

    server.RegisterHttpRequestHandler("/", HttpMethod::HEAD, say_hello);
    server.RegisterHttpRequestHandler("/", HttpMethod::GET, say_hello);
    server.RegisterHttpRequestHandler("/hello.html", HttpMethod::HEAD, send_html);
    server.RegisterHttpRequestHandler("/hello.html", HttpMethod::GET, send_html);

    try {
        std::cout << "Starting the web server.." << std::endl;
        server.Start();
        std::cout << "Server listening on " << host << ":" << port << std::endl;

        std::cout << "Enter [quit] to stop the server" << std::endl;
        std::string command;
        while (std::cin >> command, command != "quit");
        std::cout << "'quit' command entered. Stopping the web server.." << std::endl;
        server.Stop();
        std::cout << "Server stopped" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
