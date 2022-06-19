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
#include "resources/repository_cpp.hpp"

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

template<typename SendLambda>
void handle_request(http::request<http::string_body> req, SendLambda send, net::yield_context yield)
{
    const auto string_response = [&req](http::status status, std::string_view reason) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string{reason};
        res.prepare_payload();
        return res;
    };

    if (req.method() != http::verb::get) {
        return send(string_response(http::status::bad_request, "Unknown HTTP method"));
    }

    if (req.target() == "/resource") {
        return send(string_response(http::status::ok, std::string_view{libellus::repository_cpp.data(), libellus::repository_cpp.size()}));
    }

    libellus::Repository repo{"/Users/merry/Workspace/libellus", "refs/heads/main"};
    const auto files = repo.list(std::string{req.target()});

    if (!files) {
        return send(string_response(http::status::ok, "not found"));
    }

    std::string result = R"(<ul><li><a href="..">..</a></li>)";
    for (auto& f : *files) {
        result += "<li>";
        result += fmt::format(R"(<a href="{0}/">{0}</a>)", f.name);
        result += "</li>";
    }
    result += "</ul>";

    return send(string_response(http::status::ok, result));
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
