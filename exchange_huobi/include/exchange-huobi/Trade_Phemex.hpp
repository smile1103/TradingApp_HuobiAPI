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
#include "WSConnection.hpp"
#include "HttpConnection.hpp"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = net::ssl;               // from <boost/asio/ssl.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
using namespace std;

namespace exchange_phemex
{
  class TradeService : public exchange_core::TradeService, public WSEvent, public HttpEvent
  {
  public:
    TradeService(exchange_core::ExchangeConfig &config) : mConfig(config),
                                                          ws_session(std::make_shared<session>(ioc, ctx)),
                                                          http_session_(std::make_shared<HttpSession>(ioc, ctx))

    {
    }

    void Connect()
    {
      mApiKey = mConfig.GetParameter("apikey");
      mSecretKey = mConfig.GetParameter("secretkey");
      mHost = mConfig.GetParameter("host");
      mWsHost = mConfig.GetParameter("wshost");

      ws_session->add_listener(this);
      ws_session->connect(mWsHost, wsPath);

      sendAuth();
      //      sendSubscribe();

      http_session_->add_listener(this);
      http_session_->connect(mHost);

      // curl
      CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
      if (res != CURLE_OK)
      {
        spdlog::info("curl_global_init error: {}", curl_easy_strerror(res));
      }

      curl = curl_easy_init();
      if (curl == NULL)
      {
        spdlog::error("curl_easy_init error.");
      }
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20);
    }

    void RegisterListener(exchange_core::TradeEventListener &listener)
    {
      listeners.push_back(listener);
    }

    void RunOnce()
    {
      ioc.poll_one();
      long current_time = exchange_core::currentTimeInMilli();
      if ( mNeedReconnect)
      {
        Reconnect();
      }
      if ( mHttpNeedReconnect)
      {
        HttpReconnect();
      }
      if (current_time > last_heartbeat_time_ + 29000)
      {
        send_heartbeat();
        last_heartbeat_time_ = current_time;
      }
    }

    void onHttpConnect()
    {
      spdlog::info("http connected");
    }

    void onHttpMessage(const std::string &message)
    {
      if ( message.find(R"("book":)") != std::string::npos )
      {
        return;
      }
      spdlog::info("http message {}", message);
      if ( message.find(R"("code": ")") != std::string::npos )
      {
        return;
      }

      rapidjson::Document doc;
      doc.Parse(message.c_str());
      if (!doc.HasMember("code"))
      {
        return;
      }
      if (doc["code"].GetUint64() != 0)
      {
        spdlog::error("http error {}", doc["msg"].GetString());
        return;
      }

      auto &data = doc["data"];

      if (data.HasMember("execStatus"))
      {
        exchange_core::ExecutionReport er;
        er.execType = toExecType(data["execStatus"].GetString());
        er.clOrderID = data["clOrdID"].GetString();
        er.orderID = data["orderID"].GetString();
        er.symbol = data["symbol"].GetString();
        er.exchange = exchange_core::ExchangeEnum::PHEMEX;
        er.orderStatus = toOrderStatus(data["ordStatus"].GetString());

        for (exchange_core::TradeEventListener &l : listeners)
        {
          l.onExecutionReport(er);
        }
      }
    }

    void onHttpClose()
    {
      spdlog::info("http closed");
      mHttpNeedReconnect = true;
    }
    void onHttpError(error_code ec)
    {
      spdlog::info("http error ");
      mHttpNeedReconnect = true;
    }

    void onConnect()
    {
      spdlog::info("ws connected");
    }

    void onClose()
    {
    }

    void onError(error_code)
    {
      mNeedReconnect = true;
    }

    void Reconnect()
    {
      spdlog::info("reconnect websocket");
      ws_session = std::make_shared<session>(ioc, ctx);
      ws_session->add_listener(this);
      ws_session->connect(mWsHost, wsPath);
      sendAuth();
      mNeedReconnect = false;
    }

    void HttpReconnect()
    {
      spdlog::info("reconnect http");
      http_session_ = std::make_shared<HttpSession>(ioc, ctx);
      http_session_->add_listener(this);
      http_session_->connect(mHost);
      mHttpNeedReconnect = false;
    }

    void sendSubscribe()
    {
      std::string text = aopSub;
      text.replace(text.find("#"), 1, to_string(++id_));
      ws_session->send(text);
    }

    std::vector<exchange_core::TradeOrder> GetOpenOrder()
    {

      std::string url("https://" + mHost);
      std::string method = "/orders/activeList?symbol=BTCUSD";
      url.append(method);

      std::string action = "GET";

      std::vector<std::string> extra_http_header;

      std::string expiry = std::to_string(exchange_core::currentTimeInMilli() / 1000 + 60);
      auto sig_str = "/orders/activeListsymbol=BTCUSD" + expiry;
      spdlog::info("signature {}", sig_str);
      auto sig = hmac_sha256(mSecretKey.c_str(), sig_str.c_str());
      extra_http_header.push_back("x-phemex-request-signature:" + sig);
      extra_http_header.push_back("x-phemex-access-token:" + mApiKey);
      extra_http_header.push_back("x-phemex-request-expiry:" + expiry);
      extra_http_header.push_back("Content-Type: application/json");
      extra_http_header.push_back("Accept: */*");

      std::string str_result;
      std::string poststr;
      curl_api_with_header(url, str_result, extra_http_header, poststr, action);

      if (str_result.size() > 0)
      {

        try
        {
          spdlog::info(str_result);
        }
        catch (std::exception &e)
        {
          spdlog::error("cancel_order> Error ! %s", e.what());
        }
      }
      else
      {
        spdlog::error("get open order Failed to get anything");
      }

      std::vector<exchange_core::TradeOrder> result;

      return result;
    };

    void aPlaceOrder(const exchange_core::TradeOrder &order)
    {
      std::string url("https://" + mHost);
      url += "/orders";

      std::string action = "POST";

      rapidjson::StringBuffer s;
      rapidjson::Writer<rapidjson::StringBuffer> writer(s);

      long nonce = exchange_core::currentTimeInMilli();

      writer.StartObject(); // Between StartObject()/EndObject(),
      writer.Key(key_symbol.c_str());
      writer.String("BTCUSD");
      const std::string side = order.side == exchange_core::Side::BUY ? "Buy" : "Sell";

      const std::string tif = order.timeInForce == exchange_core::TimeInForce::GTC ? "GoodTillCancel" : "ImmediateOrCancel";
      const std::string exec_inst = order.orderType == exchange_core::OrderType::LIMIT_POST_ONLY ? "PostOnly" : "";

      writer.Key(key_side.c_str());
      writer.String(side.c_str());

      writer.Key(key_price.c_str());
      writer.Double(order.price);

      writer.Key(key_quantity.c_str());
      writer.Double(order.quantity);

      writer.Key(key_tif.c_str());
      writer.String(tif.c_str());

      writer.Key(key_client_oid.c_str());
      writer.String(order.clientOrderId.c_str());

      writer.EndObject();

      std::string str_result;
      std::string poststr = s.GetString();

      std::vector<std::string> extra_http_header;

      std::string expiry = std::to_string(exchange_core::currentTimeInMilli() / 1000 + 60);
      //std::string a = R"({"symbol":"BTCUSD","clOrdID":"1","side":"Buy","orderQty":10,"priceEp":95000000,"ordType":"Limit","timeInForce":"GoodTillCancel"})";
      auto sig_str = "/orders" + expiry + poststr;
      spdlog::info("signature {}", sig_str);
      auto sig = hmac_sha256(mSecretKey.c_str(), sig_str.c_str());
      spdlog::info("sig {}", sig);
      spdlog::info("api {}", mApiKey);
      extra_http_header.push_back("x-phemex-request-signature:" + sig);
      extra_http_header.push_back("x-phemex-access-token:" + mApiKey);
      extra_http_header.push_back("x-phemex-request-expiry:" + expiry);
      extra_http_header.push_back("Content-Type: application/json");
      extra_http_header.push_back("Accept: */*");

      curl_api_with_header(url, str_result, extra_http_header, poststr, action);

      if (str_result.size() > 0)
      {
        try
        {
          spdlog::info(str_result);
        }
        catch (std::exception &e)
        {
          spdlog::error("cancel_order> Error ! %s", e.what());
        }
      }
      else
      {
        spdlog::error("cancel_order Failed to get anything.");
      }
    }

    void PlaceOrder(const exchange_core::TradeOrder &order)
    {
      spdlog::info("send_order");

      std::string method = "/orders";
      std::string target = method;

      rapidjson::StringBuffer s;
      rapidjson::Writer<rapidjson::StringBuffer> writer(s);

      long nonce = exchange_core::currentTimeInMilli();

      writer.StartObject(); // Between StartObject()/EndObject(),
      writer.Key(key_symbol.c_str());
      writer.String("BTCUSD");
      const std::string side = order.side == exchange_core::Side::BUY ? "Buy" : "Sell";

      const std::string tif = order.orderType == exchange_core::OrderType::LIMIT_POST_ONLY ? "PostOnly" : order.timeInForce == exchange_core::TimeInForce::GTC ? "GoodTillCancel"
                                                                                                                                                               : "ImmediateOrCancel";

      writer.Key(key_side.c_str());
      writer.String(side.c_str());

      writer.Key(key_price.c_str());
      writer.Uint64((long)(order.price * 10000));

      writer.Key(key_quantity.c_str());
      writer.Uint64((long)order.quantity);

      writer.Key(key_tif.c_str());
      writer.String(tif.c_str());

      writer.Key(key_client_oid.c_str());
      writer.String(order.clientOrderId.c_str());

      writer.EndObject();

      std::string str_result;
      std::string poststr = s.GetString();

      spdlog::info("<create order> post_data = |{}|", poststr);
      request req;
      req.version(11);
      req.method(http::verb::post);
      req.target(target);
      req.set(http::field::host, mHost);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::content_type, "application/json");
      std::string expiry = std::to_string(exchange_core::currentTimeInMilli() / 1000 + 60);

      auto sig_str = method + expiry + poststr;
      spdlog::info("signature string {}", sig_str);
      req.set("x-phemex-request-signature", hmac_sha256(mSecretKey.c_str(), sig_str.c_str()));
      req.set("x-phemex-access-token", mApiKey);
      req.set("x-phemex-request-expiry", expiry);
      req.body() = poststr;
      req.prepare_payload();
      http_session_->send(req);
    };

    void CancelOrder(const exchange_core::TradeOrder &order)
    {
      spdlog::info("cancel_order");

      std::string param = "symbol=" + order.symbol + "&orderID=" + order.exchangeOrderId;
      std::string method = "/orders/cancel";

      request req;
      req.version(11);
      req.method(http::verb::delete_);
      req.target(method + "?" + param);
      req.set(http::field::host, mHost);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      req.set(http::field::content_type, "application/json");

      std::string expiry = std::to_string(exchange_core::currentTimeInMilli() / 1000 + 60);
      auto sig_str = method + param + expiry;
      spdlog::info("signature string {}", sig_str);
      req.set("x-phemex-request-signature", hmac_sha256(mSecretKey.c_str(), sig_str.c_str()));
      req.set("x-phemex-access-token", mApiKey);
      req.set("x-phemex-request-expiry", expiry);
      req.prepare_payload();
      http_session_->send(req);

      //ws_session->send(poststr);
    };

    void CancelAll()
    {
      spdlog::info("cancel_order");

      std::string url("https://" + mHost);
      std::string method = "/orders/all?symbol=BTCUSD";
      url.append(method);

      std::string action = "DELETE";

      std::vector<std::string> extra_http_header;

      std::string expiry = std::to_string(exchange_core::currentTimeInMilli() / 1000 + 60);
      auto sig_str = "/orders/allsymbol=BTCUSD" + expiry;
      spdlog::info("signature {}", sig_str);
      auto sig = hmac_sha256(mSecretKey.c_str(), sig_str.c_str());
      extra_http_header.push_back("x-phemex-request-signature:" + sig);
      extra_http_header.push_back("x-phemex-access-token:" + mApiKey);
      extra_http_header.push_back("x-phemex-request-expiry:" + expiry);
      extra_http_header.push_back("Content-Type: application/json");
      extra_http_header.push_back("Accept: */*");

      std::string str_result;
      std::string poststr;
      curl_api_with_header(url, str_result, extra_http_header, poststr, action);

      if (str_result.size() > 0)
      {
        try
        {
          spdlog::info(str_result);
        }
        catch (std::exception &e)
        {
          spdlog::error("cancel_order> Error ! %s", e.what());
        }
      }
      else
      {
        spdlog::error("get open order Failed to get anything");
      }
    };

  private:
    std::string hmac_sha256(const char *key, const char *data)
    {

      unsigned char *digest;
      digest = HMAC(EVP_sha256(), key, strlen(key), (unsigned char *)data, strlen(data), NULL, NULL);
      return b2a_hex((char *)digest, 32);
    }

    //------------------------------
    std::string sha256(const char *data)
    {

      unsigned char digest[32];
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      SHA256_Update(&sha256, data, strlen(data));
      SHA256_Final(digest, &sha256);
      return b2a_hex((char *)digest, 32);
    }

    std::string b2a_hex(char *byte_arr, int n)
    {

      const static std::string HexCodes = "0123456789abcdef";
      std::string HexString;
      for (int i = 0; i < n; ++i)
      {
        unsigned char BinValue = byte_arr[i];
        HexString += HexCodes[(BinValue >> 4) & 0x0F];
        HexString += HexCodes[BinValue & 0x0F];
      }
      return HexString;
    }

    void sendAuth()
    {
      std::string method = "user.auth";

      rapidjson::StringBuffer s;
      rapidjson::Writer<rapidjson::StringBuffer> writer(s);

      long nonce = exchange_core::currentTimeInMilli() / 1000 + 60;

      writer.StartObject(); // Between StartObject()/EndObject(),

      writer.Key("method");          // output a key,
      writer.String(method.c_str()); // follow by a value.
      writer.Key("params");
      writer.StartArray();

      writer.String("API");
      writer.String(mApiKey.c_str());

      std::string signature(mApiKey);
      std::string expiry = std::to_string(nonce);
      signature.append(expiry);
      std::string signaturestring = hmac_sha256(mSecretKey.c_str(), signature.c_str());

      writer.String(signaturestring.c_str());
      writer.Uint64(nonce);

      writer.EndArray();

      writer.Key("id");     // output a key,
      writer.Uint64(++id_); // follow by a value.
      auth_id_ = id_;
      writer.EndObject();

      std::string msgstr = s.GetString();

      spdlog::info("auth req {}", msgstr);
      ws_session->send(msgstr);
    }

    void onMessage(const std::string &msg)
    {
      spdlog::info("WebSocket message, {}", msg);
      rapidjson::Document doc;

      doc.Parse(msg.c_str());
      if (doc.HasMember("id"))
      {
        long id = doc["id"].GetInt64();
        if (id == auth_id_)
        {
          spdlog::info("auth accepted");
          sendSubscribe();
        }
      }

      if (doc.HasMember("accounts"))
      {
        const rapidjson::Value &ac = doc["accounts"];
        for (rapidjson::SizeType i = 0; i < ac.Size(); i++)
        {
          auto &a = ac[i];
          exchange_core::BalanceReport br;

          br.currency = a["currency"].GetString();
          br.totalBalance = a["accountBalanceEv"].GetUint64() / 1000000000.;

          for (exchange_core::TradeEventListener &l : listeners)
          {
            l.onBalanceReport(br);
          }
        }
      }
      if (doc.HasMember("orders"))
      {
        const rapidjson::Value &os = doc["orders"];
        for (rapidjson::SizeType i = 0; i < os.Size(); i++)
        {
          auto &o = os[i];

          exchange_core::ExecutionReport er;
          er.execType = toExecType(o["execStatus"].GetString());
          er.clOrderID = o["clOrdID"].GetString();
          er.orderID = o["orderID"].GetString();
          er.symbol = o["symbol"].GetString();
          er.lastExecQuantity = o["execQty"].GetUint64();
          er.lastExecPrice = o["execPriceEp"].GetUint64()/10000.;
          er.cumQuantity = o["cumQty"].GetUint64();
          er.orderStatus = toOrderStatus(o["ordStatus"].GetString());
          er.side = toSide(o["side"].GetString());
          er.exchange = exchange_core::ExchangeEnum::PHEMEX;

          for (exchange_core::TradeEventListener &l : listeners)
          {
            l.onExecutionReport(er);
          }
        }
      }
      if (doc.HasMember("positions"))
      {
        const rapidjson::Value &ps = doc["positions"];
        for (rapidjson::SizeType i = 0; i < ps.Size(); i++)
        {
          auto &p = ps[i];
          exchange_core::BalanceReport br;

          br.currency = p["symbol"].GetString();
          br.totalBalance = p["size"].GetUint64();

          std::string side = p["side"].GetString();
          if ( side == "Sell")
          {
            br.totalBalance = -br.totalBalance;
          }

          for (exchange_core::TradeEventListener &l : listeners)
          {
            l.onBalanceReport(br);
          }
        }
      }
    }

    exchange_core::Side toSide(const std::string &sidestr)
    {
      return sidestr == "Buy" ? exchange_core::Side::BUY : exchange_core::Side::SELL;
    }

    exchange_core::OrderStatus toOrderStatus(const std::string &status)
    {
      if (status == "Created")
      {
        return exchange_core::OrderStatus::PENDING_NEW;
      }
      else if (status == "New")
      {
        return exchange_core::OrderStatus::NEW;
      }

      else if (status == "Canceled" || status == "EXPIRED")
      {
        return exchange_core::OrderStatus::CANCELLED;
      }

      else if (status == "Filled")
      {
        return exchange_core::OrderStatus::FILLED;
      }
      else if (status == "Rejected")
      {
        return exchange_core::OrderStatus::REJECTED;
      }
      else if (status == "PartiallyFilled")
      {
        return exchange_core::OrderStatus::PARTIALLY_FILLED;
      }
      spdlog::error("unrecognized order status {}", status);
      return exchange_core::OrderStatus::REJECTED;
    }

    exchange_core::ExecType toExecType(const std::string str)
    {
      if (str == "Canceled")
        return exchange_core::ExecType::CANCELLED;
      else if (str == "New")
        return exchange_core::ExecType::NEW;
      else if (str == "PendingNew")
        return exchange_core::ExecType::PENDING_NEW;
      else if (str == "CreateRejected")
        return exchange_core::ExecType::REJECTED;
      else if (str == "MakerFill")
        return exchange_core::ExecType::TRADE;
      else if (str == "TakerFill")
        return exchange_core::ExecType::TRADE;
      else if (str == "Expired")
        return exchange_core::ExecType::CANCELLED;
      else if (str == "PendingCancel")
        return exchange_core::ExecType::PENDING_CANCEL;
      spdlog::error("unrecognized exec status {}", str);
      return exchange_core::ExecType::REJECTED;
    }

    exchange_core::OrderType toOrderType(const std::string str)
    {
      if (str == "LIMIT")
        return exchange_core::OrderType::LIMIT;
      else
        return exchange_core::OrderType::MARKET;
    }

    static size_t curl_cb(void *content, size_t size, size_t nmemb, std::string *buffer)
    {

      buffer->append((char *)content, size * nmemb);

      return size * nmemb;
    }

    void curl_api_with_header(std::string &url, std::string &str_result, std::vector<std::string> &extra_http_header, std::string &post_data, std::string &action)
    {

      CURLcode res;

      if (curl)
      {
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str_result);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");

        if (extra_http_header.size() > 0)
        {

          struct curl_slist *chunk = NULL;
          for (int i = 0; i < extra_http_header.size(); i++)
          {
            chunk = curl_slist_append(chunk, extra_http_header[i].c_str());
          }
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        if (post_data.size() > 0 || action == "POST" || action == "PUT" || action == "DELETE")
        {

          if (action == "PUT" || action == "DELETE")
          {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, action.c_str());
          }
          curl_easy_setopt(curl, CURLOPT_POST, 1); //out of if data and post = 1 or 0,
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }

        res = curl_easy_perform(curl);

        /* Check for errors */
        if (res != CURLE_OK)
        {
          spdlog::error("<url_api> curl_easy_perform() failed: {}", curl_easy_strerror(res));
        }
      }
    }

    void send_heartbeat()
    {
      std::string text = heartBeat;
      text.replace(text.find("#"), 1, to_string(id_++));
      spdlog::info("send heartbeat {}", text);
      ws_session->send(text);

      std::string method = "/md/orderbook?symbol=BTCUSD";
      std::string target = method;
      request req;
      req.version(11);
      req.method(http::verb::get);
      req.target(target);
      req.set(http::field::host, mHost);
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
      http_session_->send(req);
    }

    std::string mHost;
    std::string mApiKey;
    std::string mSecretKey;
    std::string mWsHost;

    exchange_core::ExchangeConfig &mConfig;

    std::vector<std::reference_wrapper<exchange_core::TradeEventListener>> listeners;

    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};

    const std::string key_symbol{"symbol"};
    const std::string key_side{"side"};
    const std::string key_price{"priceEp"};
    const std::string key_quantity{"orderQty"};
    const std::string key_client_oid{"clOrdID"};
    const std::string key_tif{"timeInForce"};

    const std::string key_order_id{"order_id"};

    const std::string wsPath{"/ws"};

    string const heartBeat = R"({"id":#, "method":"server.ping", "params":[]})";
    string const aopSub = R"({"method":"aop.subscribe", "params":[],"id":#})";

    std::shared_ptr<session> ws_session;
    std::shared_ptr<HttpSession> http_session_;

    bool mNeedReconnect{false};
    bool mHttpNeedReconnect{false};
    long last_heartbeat_time_{0};

    CURL *curl = NULL;

    long id_{1000};
    long auth_id_;
  };
}
