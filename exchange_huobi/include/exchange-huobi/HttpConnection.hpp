#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace beast = boost::beast;   // from <boost/beast.hpp>
namespace http = beast::http;     // from <boost/beast/http.hpp>
namespace net = boost::asio;      // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using request = http::request<http::string_body>;

namespace exchange_phemex
{
  class HttpEvent {
  public:
    virtual void onHttpConnect() = 0;
    virtual void onHttpMessage(const std::string & message ) = 0;
    virtual void onHttpClose() = 0;
    virtual void onHttpError(error_code ec) = 0;
  };

  class HttpSession : public std::enable_shared_from_this<HttpSession>
  {
    tcp::resolver resolver_;
    beast::ssl_stream<beast::tcp_stream> stream_;
    beast::flat_buffer buffer_; // (Must persist between reads)
    http::request<http::empty_body> req_;
    http::response<http::string_body> res_;

    std::vector<HttpEvent *> listeners_;

  public:
    explicit HttpSession(net::io_context& ioc, ssl::context& ctx)
        : resolver_(net::make_strand(ioc))
        , stream_(net::make_strand(ioc), ctx)
    {
    }

    void add_listener(HttpEvent * listener)
    {
      listeners_.push_back(listener);
    }

    // Start the asynchronous operation
    void
    connect(
        const std::string & host)
    {
      // Set SNI Hostname (many hosts need this to handshake successfully)
      if (!SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
      {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << ec.message() << "\n";
        return;
      }

      // Set up an HTTP GET request message
//      req_.version(version);
//    req_.method(http::verb::get);
//      req_.target(target);
//      req_.set(http::field::host, host);
//      req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

      // Look up the domain name
      resolver_.async_resolve(
          host.c_str(),
          "443",
          beast::bind_front_handler(
              &HttpSession::on_resolve,
              shared_from_this()));
    }

    void
    on_resolve(
        beast::error_code ec,
        tcp::resolver::results_type results)
    {
      if (ec)
        return fail(ec, "resolve");

      // Set a timeout on the operation
      beast::get_lowest_layer(stream_).expires_never();

      // Make the connection on the IP address we get from a lookup
      spdlog::info("connecting http");
      beast::get_lowest_layer(stream_).async_connect(
          results,
          beast::bind_front_handler(
              &HttpSession::on_connect,
              shared_from_this()));
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
      if (ec)
        return fail(ec, "connect");

      // Perform the SSL handshake
      stream_.async_handshake(
          ssl::stream_base::client,
          beast::bind_front_handler(
              &HttpSession::on_handshake,
              shared_from_this()));
    }

    void
    on_handshake(beast::error_code ec)
    {
      if (ec)
        return fail(ec, "handshake");

      // Set a timeout on the operation
//      beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      mState = connected;
      check_read();
      check_send();
      for ( auto l : listeners_)
      {
          l->onHttpConnect();
      }
    }

    void
    on_shutdown(beast::error_code ec)
    {
      if (ec == net::error::eof)
      {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec = {};
      }
      if (ec)
        return fail(ec, "shutdown");

      // If we get here then the connection is closed gracefully
    }

    void check_read()
    {
      http::async_read(stream_, buffer_, res_,
            beast::bind_front_handler(
                &HttpSession::handle_read,
                shared_from_this()));
    }

    void handle_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
      if (ec)
      {
        fail(ec, "read error");
      }
      else
      {
        boost::ignore_unused(bytes_transferred);
        // handle the read here

        for ( auto l : listeners_)
        {
          l->onHttpMessage(res_.body());
        }
        buffer_.consume(buffer_.size());
        res_.body()="";
        // keep reading until error
        check_read();

        //        send(std::move(message));
      }
    }

    void send(request & msg)
    {
      request_queue_.push_back(std::move(msg));
      check_send();
    }

    void check_send()
    {
      if (mState != connected || mSending_state == sending || request_queue_.empty())
        return;

      initiate_tx();
    }

    void initiate_tx()
    {
      mSending_state = sending;
      http::async_write(stream_, request_queue_.front(), 
        beast::bind_front_handler(&HttpSession::handle_tx,
                shared_from_this()));
    }

    void handle_tx(error_code ec, 
      std::size_t bytes_transferred)
    {
      if (ec)
      {
        fail(ec, "failed to send message: ");
      }
      else
      {
        request_queue_.pop_front();
        mSending_state = send_idle;
        check_send();
      }
    }

    void
    on_close(beast::error_code ec)
    {
      for ( auto l : listeners_)
      {
        l->onHttpClose();
      }
      if (ec)
        return fail(ec, "close");
    }

    void fail(error_code ec, const std::string &msg)
    {
      spdlog::error("{}: {}", msg, ec.message());
      for ( auto l : listeners_)
      {
        l->onHttpError(ec);
      }

    }

  private:
    beast::flat_buffer rd_buffer;
    std::deque<request> request_queue_;
    enum
    {
      handshaking,
      connected,
    } mState = handshaking;

    enum
    {
      send_idle,
      sending
    } mSending_state = send_idle;
  };

}