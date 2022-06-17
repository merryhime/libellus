
#include <memory>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <fmt/format.h>
#include <mcl/assert.hpp>
#include <mcl/stdint.hpp>

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

struct Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket&& socket_)
        : socket{std::move(socket_)}
    {}

    void run()
    {
        do_read();
    }

private:
    void do_read()
    {
        fmt::print("lolololololol\n");
    }

    tcp::socket socket;
};

struct Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context& ioc_, net::ip::address addr_, u16 port_)
        : ioc{ioc_}
        , acceptor{ioc_}
    {
        beast::error_code ec;
        tcp::endpoint endpoint{addr_, port_};

        acceptor.open(endpoint.protocol(), ec);
        ASSERT_MSG(!ec, "acceptor.open {}", ec.message());

        acceptor.set_option(net::socket_base::reuse_address(true), ec);
        ASSERT_MSG(!ec, "acceptor.set_option {}", ec.message());

        acceptor.bind(endpoint, ec);
        ASSERT_MSG(!ec, "acceptor.bind {}", ec.message());

        acceptor.listen(net::socket_base::max_listen_connections, ec);
        ASSERT_MSG(!ec, "acceptor.listen {}", ec.message());
    }

    void run()
    {
        acceptor.async_accept(ioc, [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            ASSERT_MSG(!ec, "accept failure {}", ec.message());

            std::make_shared<Session>(std::move(socket))->run();

            self->run();
        });
    }

private:
    net::io_context& ioc;
    tcp::acceptor acceptor;
};

int main()
{
    const auto addr = net::ip::make_address("0.0.0.0");
    const u16 port = 54321;

    net::io_context ioc{};

    std::make_shared<Listener>(ioc, addr, port)->run();

    ioc.run();

    return 0;
}
