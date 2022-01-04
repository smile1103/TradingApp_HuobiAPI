#pragma once
#include <exchange-core/exchange-core.h>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/spawn.hpp>
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
      spdlog::info("WebSocket message, {}", msg);

      rapidjson::Document doc;

      doc.Parse(msg.c_str());

      if (doc.HasMember("book"))
      {
        auto & book = doc["book"];
        std::string type = doc["type"].GetString();
        std::string symbol = doc["symbol"].GetString();

        if ( type == "snapshot")
        {
           auto iter = snapshot_map_.find(symbol);

          if ( iter == snapshot_map_.end())
          {
            snapshot_map_.insert(std::make_pair(symbol, snapshot()));
            iter = snapshot_map_.find(symbol);
          }

          auto & s = iter->second;
          s.bids.clear();
          s.asks.clear();
          const rapidjson::Value &bids = book["bids"];
          for (rapidjson::SizeType i = 0; i < bids.Size(); i++)
          {
            const rapidjson::Value &d = bids[i];
            int price = d[0].GetInt64();
            int qty = d[1].GetInt64();
            s.bids[price] = qty;
          }
          const rapidjson::Value &asks = book["asks"];
          for (rapidjson::SizeType i = 0; i < asks.Size(); i++)
          {
            const rapidjson::Value &d = asks[i];
            int price = d[0].GetInt64();
            int qty = d[1].GetInt64();
            s.asks[price] = qty;
          }
        }
        else if ( type == "incremental")
        {
          auto iter = snapshot_map_.find(symbol);

          if ( iter == snapshot_map_.end())
          {
            spdlog::error("cannot find snapshot for {}", symbol);
          }

          auto & s = iter->second;
          const rapidjson::Value &bids = book["bids"];
          for (rapidjson::SizeType i = 0; i < bids.Size(); i++)
          {
            const rapidjson::Value &d = bids[i];
            int price = d[0].GetInt64();
            int qty = d[1].GetInt64();
            if ( qty == 0)
            {
              s.bids.erase(price);
            }
            else {
              s.bids[price] = qty;
            }
          }
          const rapidjson::Value &asks = book["asks"];
          for (rapidjson::SizeType i = 0; i < asks.Size(); i++)
          {
            const rapidjson::Value &d = asks[i];
            int price = d[0].GetInt64();
            int qty = d[1].GetInt64();
            s.asks[price] = qty;
            if ( qty == 0)
            {
              s.asks.erase(price);
            }
            else {
              s.asks[price] = qty;
            }
          }


        }

        exchange_core::OrderBook orderBook;
        orderBook.exchange = exchange_core::ExchangeEnum::PHEMEX;
        orderBook.timestamp = doc["timestamp"].GetInt64()/1000000;
        orderBook.instrumentId = symbolToId[symbol];
        exchange_core::FeedPriceLevel level;
        level.price = snapshot_map_[symbol].bids.rbegin()->first/10000.;
        level.quantity = snapshot_map_[symbol].bids.rbegin()->second;
        level.number_of_order = 1;
        orderBook.bids.push_back(level);
        level.price = snapshot_map_[symbol].asks.begin()->first/10000.;
        level.quantity = snapshot_map_[symbol].asks.begin()->second;
        level.number_of_order = 1;
        orderBook.asks.push_back(level);

        for (auto listener : listeners)
        {
          listener->onOrderBook(orderBook);
        }
      }
      if (doc.HasMember("trades"))
      {
        auto & trades = doc["trades"];
        std::string type = doc["type"].GetString();
        std::string symbol = doc["symbol"].GetString();

        for (rapidjson::SizeType i = 0; i < trades.Size(); i++)
        {
          const rapidjson::Value &d = trades[i];
          exchange_core::FeedTrade trade;
          trade.exchange = exchange_core::ExchangeEnum::PHEMEX;
          trade.price = d[2].GetInt64()/10000.;
          trade.quantity = d[3].GetInt64();
          trade.side = d[1].GetString() == "Sell" ? exchange_core::Side::SELL : exchange_core::Side::BUY;
          trade.timestamp = d[0].GetInt64()/1000000;
          trade.instrument_id = symbolToId[symbol];
  
          for (auto listener : listeners)
          {
            listener->onTrade(trade);
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
    std::string text = heartBeat;
    text.replace(text.find("#"), 1, to_string(id_++));
    spdlog::info("send heartbeat {}", text);
    ws_session->send(text);
  }

  std::vector<exchange_core::MarketDataEventListener *> listeners;
  //    vector<exchange_core::Instrument> > subscribes_;

  net::io_context ioc;
  ssl::context ctx{ssl::context::tlsv12_client};
  std::shared_ptr<session> ws_session;
  bool needReconnect_{false};
  string const host_ = "phemex.com";
  string const port_ = "443";
  string const path_ = "/ws";
  string const bookSub = R"({"id":#, "method":"orderbook.subscribe", "params":["BTCUSD"]})";
  string const tradeSub = R"({"id":#, "method":"trade.subscribe", "params":["BTCUSD"]})";
  string const heartBeat = R"({"id":#, "method":"server.ping", "params":[]})";
  std::unordered_map<std::string, int> symbolToId;

  long last_heartbeat_time_{0};
  long id_ = 1;

  std::unordered_map<std::string, snapshot> snapshot_map_;
};
}
