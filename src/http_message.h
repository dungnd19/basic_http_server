//
// Created by dungnd on 07/06/2022.
//

#ifndef BASIC_HTTP_SERVER_HTTP_MESSAGE_H
#define BASIC_HTTP_SERVER_HTTP_MESSAGE_H

#include <string>
#include <utility>
#include <map>
#include "uri.h"

namespace basic_http_server {

    // HTTP method https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods
    enum class HttpMethod {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATCH
    };

    enum class HttpVersion {
        HTTP_0_9 = 9,
        HTTP_1_0 = 10,
        HTTP_1_1 = 11,
        HTTP_2_0 = 20
    };

    // Http status code https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
    enum class HttpStatusCode {
        // Information responses (1xx)
        Continue = 100,
        SwitchingProtocols = 101,
        EarlyHints = 103,
        // Successful responses (2xx)
        Ok = 200,
        Created = 201,
        Accepted = 202,
        NonAuthoritativeInformation = 203,
        NoContent = 204,
        ResetContent = 205,
        PartialContent = 206,
        // Redirection messages (3xx)
        MultipleChoices = 300,
        MovedPermanently = 301,
        Found = 302,
        SeeOther = 303,
        NotModified = 304,
        TemporaryRedirect = 307,
        PermanentRedirect = 308,
        // Client error responses (4xx)
        BadRequest = 400,
        Unauthorized = 401,
        Forbidden = 403,
        NotFound = 404,
        MethodNotAllowed = 405,
        NotAcceptable = 406,
        ProxyAuthenticationRequired = 407,
        RequestTimeout = 408,
        Conflict = 409,
        Gone = 410,
        LengthRequired = 411,
        PreconditionFailed = 412,
        PayloadTooLarge = 413,
        URITooLong = 414,
        UnsupportedMediaType = 415,
        RangeNotSatisfiable = 416,
        ExpectationFailed = 417,
        ImATeapot = 418,
        UpgradeRequired = 426,
        PreconditionRequired = 428,
        TooManyRequests = 429,
        RequestHeaderFieldsTooLarge = 431,
        UnavailableForLegalReasons = 451,
        // Server error responses (5xx)
        InternalServerError = 500,
        NotImplemented = 501,
        BadGateway = 502,
        ServiceUnvailable = 503,
        GatewayTimeout = 504,
        HttpVersionNotSupported = 505,
        VariantAlsoNegotiates = 506,
        NotExtended = 510,
        NetworkAuthenticationRequired = 511
    };

    // Convert functions between enum classes and string
    std::string to_string(HttpMethod method);
    std::string to_string(HttpVersion version);
    std::string to_string(HttpStatusCode status_code);
    HttpMethod string_to_method(const std::string& method_string);
    HttpVersion string_to_version(const std::string& version_string);

    // Defines the common interface of an HTTP request and HTTP response.
    // Interface contains HTTP version, collection of header fields, message content
    class HttpMessageInterface {
    public:
        HttpMessageInterface() : version_(HttpVersion::HTTP_1_1) {}
        virtual ~HttpMessageInterface() = default;

        void SetHeader(const std::string& key, const std::string& value) { headers_[key] = std::move(value); }
        void RemoveHeader(const std::string& key) { headers_.erase(key); }
        void ClearHeader() { headers_.clear(); }
        void SetContent(const std::string& content) {
            content_ = std::move(content);
            SetContentLength();
        }
        void ClearContent(const std::string& content) {
            content_.clear();
            SetContentLength();
        }

        HttpVersion version () const { return version_; }
        std::string header(const std::string& key) const {
            if (headers_.count(key) > 0)
                return headers_.at(key);
            return std::string();
        }
        std::map<std::string, std::string> headers() const { return headers_; }
        std::string content() const { return content_; }
        size_t content_length() const { return content_.length(); }

    protected:
        HttpVersion version_;
        std::map<std::string, std::string> headers_;
        std::string content_;

        void SetContentLength() { SetHeader("Content-Length", std::to_string(content_.length())); }
    };

    // A Http request
    // Contain HTTP method, URI, HttpMessageInterface
    class HttpRequest : public HttpMessageInterface {
    public:
        HttpRequest() : method_(HttpMethod::GET){}
        ~HttpRequest() = default;

        void SetMethod(HttpMethod& method) {method_ = method;}
        void SetUri(const Uri& uri) {uri_ = std::move(uri);}

        HttpMethod method() const {return method_;}
        Uri uri() const {return  uri_;}

        friend std::string to_string(const HttpRequest& request);
        friend HttpRequest string_to_request(const std::string& request_string);
    private:
        HttpMethod method_;
        Uri uri_;
    };

    // A HTTP response
    // Contain HTTP status code, HttpMessageInterface
    class HttpResponse : public HttpMessageInterface {
    public:
        HttpResponse() : status_code_(HttpStatusCode::Ok) {}
        HttpResponse(HttpStatusCode status_code) : status_code_(status_code) {}
        ~HttpResponse() = default;

        void SetStatusCode(HttpStatusCode status_code) { status_code_ = status_code; }

        HttpStatusCode status_code() const { return status_code_; }

        friend std::string to_string(const HttpResponse& response, bool send_content);
        friend HttpResponse string_to_response(const std::string& response_string);

    private:
        HttpStatusCode status_code_;
    };

    // Functions to convert HTTP message objects to string
    std::string to_string(const HttpRequest& request);
    std::string to_string(const HttpResponse& response, bool send_content);
    HttpRequest string_to_request(const std::string& request_string);
    HttpResponse string_to_response(const std::string& response_string);
}

#endif //BASIC_HTTP_SERVER_HTTP_MESSAGE_H
