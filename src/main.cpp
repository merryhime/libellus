#include <memory>
#include <string_view>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <mcl/assert.hpp>
#include <mcl/stdint.hpp>

#include "repository.hpp"
#include "resources/static/static_resources.hpp"

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>beast::string_view

beast::string_view mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if (pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();

    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".mjs"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    if (iequals(ext, ".ttf"))
        return "font/ttf";
    if (iequals(ext, ".woff"))
        return "font/woff";
    if (iequals(ext, ".woff2"))
        return "font/woff2";
    return "application/octet-stream";
}

template<typename SendLambda>
void handle_request(http::request<http::string_body> req, SendLambda send, net::yield_context yield)
{
    const auto string_response = [&req](http::status status, beast::string_view content_type, std::string_view body) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::string{body};
        res.prepare_payload();
        return res;
    };

    if (req.method() != http::verb::get) {
        return send(string_response(http::status::bad_request, "text/plain", "Unknown HTTP method"));
    }

    if (req.target().starts_with("/static/")) {
        const auto& static_map = libellus::resources::static_resources_map;
        const auto map_key = "resources" + std::string{req.target()};
        if (auto iter = static_map.find(map_key); iter != static_map.end()) {
            return send(string_response(http::status::ok, mime_type(map_key), std::string_view{(const char*)iter->second.data(), iter->second.size()}));
        }
        return send(string_response(http::status::not_found, "text/plain", "static file not found"));
    }

    libellus::Repository repo{"/Users/merry/Workspace/libellus", "refs/heads/main"};
    const auto files = repo.list(std::string{req.target()});

    if (!files) {
        return send(string_response(http::status::ok, "text/plain", "not found"));
    }

    std::string result = R"(<ul><li><a href="..">..</a></li>)";
    for (auto& f : *files) {
        result += "<li>";
        result += fmt::format(R"(<a href="{0}/">{0}</a>)", f.name);
        result += "</li>";
    }
    result += "</ul>";

    return send(string_response(http::status::ok, "text/html", result));
}

void do_session(net::io_context& ioc, beast::tcp_stream stream, net::yield_context yield)
{
    beast::error_code ec;

    beast::flat_buffer buf;
    http::request<http::string_body> req;
    bool close = false;

    const auto send_lambda = [&]<typename Msg>(Msg&& msg) {
        close = msg.need_eof();

        using is_request = typename Msg::is_request;
        using body_type = typename Msg::body_type;
        using fields_type = typename Msg::fields_type;
        http::serializer<is_request::value, body_type, fields_type> ser{msg};

        http::async_write(stream, ser, yield[ec]);
    };

    for (;;) {
        http::async_read(stream, buf, req, yield[ec]);

        if (ec == http::error::end_of_stream)
            break;

        if (ec) {
            fmt::print("session::do_read error: {}\n", ec.message());
            break;
        }

        handle_request(req, send_lambda, yield);

        ASSERT_MSG(!ec, "write failure {}", ec.message());

        if (close)
            break;
    }

    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    ASSERT_MSG(!ec, "session::do_close {}", ec.message());
}

void do_listen(net::io_context& ioc, net::ip::address addr, u16 port, net::yield_context yield)
{
    beast::error_code ec;

    tcp::endpoint endpoint{addr, port};
    tcp::acceptor acceptor{ioc};

    acceptor.open(endpoint.protocol(), ec);
    ASSERT_MSG(!ec, "acceptor.open {}", ec.message());

    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    ASSERT_MSG(!ec, "acceptor.set_option {}", ec.message());

    acceptor.bind(endpoint, ec);
    ASSERT_MSG(!ec, "acceptor.bind {}", ec.message());

    acceptor.listen(net::socket_base::max_listen_connections, ec);
    ASSERT_MSG(!ec, "acceptor.listen {}", ec.message());

    for (;;) {
        tcp::socket socket{ioc};

        acceptor.async_accept(socket, yield[ec]);
        ASSERT_MSG(!ec, "accept failure {}", ec.message());

        boost::asio::spawn(ioc, [&](net::yield_context yield) {
            do_session(ioc, beast::tcp_stream{std::move(socket)}, yield);
        });
    }
}

int main()
{
    const auto addr = net::ip::make_address("0.0.0.0");
    const u16 port = 54321;

    net::io_context ioc{};

    boost::asio::spawn(ioc, [&](net::yield_context yield) {
        do_listen(ioc, addr, port, yield);
    });

    ioc.run();

    return 0;
}
