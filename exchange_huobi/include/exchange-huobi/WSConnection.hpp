#pragma once
#include <exchange-core/exchange-core.h>
#include <functional>
#include <spdlog/spdlog.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
#include <boost/asio/strand.hpp>
#include <functional>
#include <queue>
#include <deque>
#include <memory>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = net::ssl;               // from <boost/asio/ssl.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
using namespace std;

namespace exchange_phemex
{

  class WSEvent {
  public:
    virtual void onConnect() = 0;
    virtual void onMessage(const std::string & message ) = 0;
    virtual void onClose() = 0;
    virtual void onError(error_code ec) = 0;
  };

  class session : public std::enable_shared_from_this<session>
  {
    tcp::resolver resolver_;
    websocket::stream<
        beast::ssl_stream<beast::tcp_stream>>
        ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string path_;

    std::vector<WSEvent *> listeners;

  public:
    // Resolver and socket require an io_context
    explicit session(net::io_context &ioc, ssl::context &ctx)
        : resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc), ctx)
    {
    }

    void add_listener(WSEvent * listener)
    {
      listeners.push_back(listener);
    }
    // Start the asynchronous operation
    void
    connect(
        const std::string &host,
        const std::string &path)
    {
      // Save these for later
      host_ = host;
      path_ = path;

      // Look up the domain name
      resolver_.async_resolve(
          host.c_str(),
          "443",
          beast::bind_front_handler(
              &session::on_resolve,
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
      beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

      // Make the connection on the IP address we get from a lookup
      beast::get_lowest_layer(ws_).async_connect(
          results,
          beast::bind_front_handler(
              &session::on_connect,
              shared_from_this()));
    }

    void
    on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
    {
      if (ec)
        return fail(ec, "connect");

      // Update the host_ string. This will provide the value of the
      // Host HTTP header during the WebSocket handshake.
      // See https://tools.ietf.org/html/rfc7230#section-5.4
      //      host_ += ':' + std::to_string(ep.port());

      // Set a timeout on the operation
      beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

      // Set SNI Hostname (many hosts need this to handshake successfully)
      
      if (!SSL_set_tlsext_host_name(
              ws_.next_layer().native_handle(),
              host_.c_str()))
      {
        ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                               net::error::get_ssl_category());
        return fail(ec, "connect");
      }

      // Perform the SSL handshake
      ws_.next_layer().async_handshake(
          ssl::stream_base::client,
          beast::bind_front_handler(
              &session::on_ssl_handshake,
              shared_from_this()));
      // websocket::permessage_deflate opt;
      // opt.client_enable = true; // for clients
      // opt.server_enable = true; // for servers
      // s.set_option(opt);

      // s.handshake(host + ":" + port, path);
    }

    void
    on_ssl_handshake(beast::error_code ec)
    {
      if (ec)
        return fail(ec, "ssl_handshake");

      // Turn off the timeout on the tcp_stream, because
      // the websocket stream has its own timeout system.
      beast::get_lowest_layer(ws_).expires_never();

      // Set suggested timeout settings for the websocket
      ws_.set_option(
          websocket::stream_base::timeout::suggested(
              beast::role_type::client));

      // Set a decorator to change the User-Agent of the handshake
      ws_.set_option(websocket::stream_base::decorator(
          [](websocket::request_type &req)
          {
            req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async-ssl");
          }));

      // websocket::permessage_deflate opt;
      // opt.client_enable = true; // for clients
      // ws_.set_option(opt);
// Perform the websocket handshake
      ws_.async_handshake(host_, path_,
                          beast::bind_front_handler(
                              &session::on_handshake,
                              shared_from_this()));
    }

   void
      on_handshake(beast::error_code ec)
    {
      if (ec)
        return fail(ec, "handshake");

      std::this_thread::sleep_for (std::chrono::seconds(1));  
      mState = connected;
      check_read();
      check_send();
      for ( auto l : listeners)
      {
          l->onConnect();
      }
    }

    void check_read()
    {
      ws_.async_read(rd_buffer, [self = this->shared_from_this()](error_code ec, std::size_t bytes_transferred)
                    { self->handle_read(ec, bytes_transferred); });
    }
    void handle_read(error_code ec, std::size_t bytes_transferred)
    {
      if (ec)
      {
        fail(ec, "read error");
      }
      else
      {
        // handle the read here
        
        auto message = beast::buffers_to_string(rd_buffer.data());
        for ( auto l : listeners)
        {
          l->onMessage(message);
        }
        rd_buffer.consume(message.size());

        // keep reading until error
        check_read();

        //        send(std::move(message));
      }
    }
    void send(std::string msg)
    {
      write_queue.push(std::move(msg));
      check_send();
    }

    void check_send()
    {
      if (mState != connected || mSending_state == sending || write_queue.empty())
        return;

      initiate_tx();
    }

    void initiate_tx()
    {
      mSending_state = sending;
      spdlog::info("send websocket message {}", write_queue.front());
      ws_.async_write(net::buffer(write_queue.front()), [self = shared_from_this()](error_code ec, std::size_t)
                     {
                       // we don't care about bytes_transferred
                       self->handle_tx(ec);
                     });
    }

    void handle_tx(error_code ec)
    {
      if (ec)
      {
        fail(ec, "failed to send message: ");
      }
      else
      {
        write_queue.pop();
        mSending_state = send_idle;
        check_send();
      }
    }

    void
    on_close(beast::error_code ec)
    {
      for ( auto l : listeners)
      {
        l->onClose();
      }
      if (ec)
        return fail(ec, "close");
    }

    void fail(error_code ec, const std::string &msg)
    {
      spdlog::error("{}: {}", msg, ec.message());
      for ( auto l : listeners)
      {
        l->onError(ec);
      }

    }

  private:
    beast::flat_buffer rd_buffer;
    std::queue<std::string, std::deque<std::string>> write_queue;

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