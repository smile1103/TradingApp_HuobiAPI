#pragma once
#include <exchange-core/exchange-core.h>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <string>
#include <sstream>
#include <thread>
#include <rapidjson/document.h>
#include <chrono>
#include <string>
#include <algorithm>
#include "WSConnection.hpp"
#include <map>
#include <unordered_map>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace exchange_phemex
{
  struct snapshot
  {
    std::map<int, int> bids;
    std::map<int, int> asks;
  };

  class MarketDataService : public exchange_core::MarketDataService, public WSEvent
  {
    public:
      MarketDataService()
      {
      }
      ~MarketDataService()
      {
      }

      virtual void Connect()
      {
        symbolToId["BTCUSD"] = 1;
        symbolToId["BTC_USDT"] = 1;
        symbolToId["BTCUSD"] = 1;

        ws_session = std::make_shared<session>(ioc, ctx);
        ws_session->add_listener(this);
        ws_session->connect(host_, path_);


      }

      void RunOnce()
      {
        ioc.poll_one();
        long current_time = exchange_core::currentTimeInMilli();
        if (current_time > last_heartbeat_time_ + 29000)
        {
          send_heartbeat();
          last_heartbeat_time_ = current_time;
        }
      }

      virtual void Subscribe(exchange_core::Instrument &instr)
      {
        //      subscribes.push_back(instr);
      }
      virtual void Unsubscribe(exchange_core::Instrument &instr)
      {
      }

      void RegisterListener(exchange_core::MarketDataEventListener &listener)
      {
        listeners.push_back(&listener);
      }

      void onConnect()
      {
        spdlog::info("ws connected");
        sendSubscription();
      }

      void onClose()
      {
      }

      void onError(error_code)
      {
        needReconnect_ = true;
      }

      void onMessage(const string &msg)
      {
        namespace bio = boost::iostreams;

        std::stringstream compressed(msg);
        std::stringstream decompressed;

        bio::filtering_streambuf<bio::input> out;
        out.push(bio::gzip_decompressor());
        out.push(compressed);
        bio::copy(out, decompressed);
        std::string message = decompressed.str();
        spdlog::info("WebSocket message, {}", message);
        rapidjson::Document doc;
        doc.Parse(message.c_str());
        if (doc.HasMember("ping"))
        {
          int ping_id = doc["ping"].GetInt64();
          std::string text = heartBeat;
          text.replace(text.find("#"), 1, to_string(ping_id));
          spdlog::info("send heartbeat {}", text);
          ws_session->send(text);
        }
        if (doc.HasMember("ch"))
        {
          std::string symbol = "BTCUSD";
          std::string type = doc["ch"].GetString();
          if(type == "market.btcusdt.depth.step0")
          {
            auto &tick = doc["tick"];
            auto iter = snapshot_map_.find(symbol);
            if ( iter == snapshot_map_.end())
            {
              snapshot_map_.insert(std::make_pair(symbol, snapshot()));
              iter = snapshot_map_.find(symbol);
            }
            auto & s = iter->second;
            s.bids.clear();
            s.asks.clear();
            const rapidjson::Value &bids = tick["bids"];
            for (rapidjson::SizeType i = 0; i < bids.Size(); i++)
            {
              const rapidjson::Value &d = bids[i];
              int price = d[0].GetDouble();
              int qty = d[1].GetDouble();
              s.bids[price] = qty;
            }
            const rapidjson::Value &asks = tick["asks"];
            for (rapidjson::SizeType i = 0; i < asks.Size(); i++)
            {
              const rapidjson::Value &d = asks[i];
              int price = d[0].GetDouble();
              int qty = d[1].GetDouble();
              s.asks[price] = qty;
            }
            exchange_core::OrderBook orderBook;
            orderBook.exchange = exchange_core::ExchangeEnum::HUOBI;
            orderBook.timestamp = doc["ts"].GetInt64()/1000000;
            orderBook.instrumentId = symbolToId[symbol];
            exchange_core::FeedPriceLevel level;
            level.price = snapshot_map_[symbol].bids.rbegin()->first;
            level.quantity = snapshot_map_[symbol].bids.rbegin()->second;
            level.number_of_order = 1;
            orderBook.bids.push_back(level);
            level.price = snapshot_map_[symbol].asks.begin()->first;
            level.quantity = snapshot_map_[symbol].asks.begin()->second;
            level.number_of_order = 1;
            orderBook.asks.push_back(level);

            for (auto listener : listeners)
            {
              listener->onOrderBook(orderBook);
            }
          }
          if(type == "market.btcusdt.trade.detail")
          {
            auto &tick = doc["tick"];
            const rapidjson::Value &trades = tick["data"];
            for (rapidjson::SizeType i = 0; i < trades.Size(); i++)
            {
              const rapidjson::Value &d = trades[i];
              exchange_core::FeedTrade trade;
              trade.exchange = exchange_core::ExchangeEnum::HUOBI;
              trade.price = d["price"].GetDouble();
              trade.quantity = d["amount"].GetDouble();
              trade.side = d["direction"].GetString() == "sell" ? exchange_core::Side::SELL : exchange_core::Side::BUY;
              trade.timestamp = d["ts"].GetInt64()/1000000;
              trade.instrument_id = symbolToId[symbol];
              for (auto listener : listeners)
              {
                listener->onTrade(trade);
              }
            }
          }
        }
      }

    private :
    void sendSubscription()
    {
      long id = exchange_core::currentTimeInMilli();
      std::string text = bookSub;
      text.replace(text.find("#"), 1, to_string(id));
      spdlog::info("send book sub {}", text);
      ws_session->send(text);
      text = tradeSub;
      text.replace(text.find("#"), 1, to_string(++id));
      spdlog::info("send trade sub {}", text);
      ws_session->send(text);
    }


    void send_heartbeat()
    {
      // std::string text = heartBeat;
      // text.replace(text.find("#"), 1, to_string(id_++));
      // spdlog::info("send heartbeat {}", text);
      // ws_session->send(text);
    }

    std::vector<exchange_core::MarketDataEventListener *> listeners;
    //    vector<exchange_core::Instrument> > subscribes_;

    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    std::shared_ptr<session> ws_session;
    bool needReconnect_{false};
    string const host_ = "api.huobi.pro";
    string const port_ = "443";
    string const path_ = "/ws";
    string const bookSub = R"({"sub": "market.btcusdt.depth.step0", "id":#})"; // you can change depth type : https://huobiapi.github.io/docs/spot/v1/en/#market-depth
    string const tradeSub = R"({"sub": "market.btcusdt.trade.detail", "id":#})"; 
    // string const bookSub = R"({"id":#, "method":"orderbook.subscribe", "params":["BTCUSD"]})";
    // string const tradeSub = R"({"id":#, "method":"trade.subscribe", "params":["BTCUSD"]})";
    string const heartBeat = R"({"pong":#})";
    std::unordered_map<std::string, int> symbolToId;

    long last_heartbeat_time_{0};
    long id_ = 1;

    std::unordered_map<std::string, snapshot> snapshot_map_;
  };
}
