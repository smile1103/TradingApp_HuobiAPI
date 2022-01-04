#pragma once
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include "exchange-phemex/exchange-phemex.h"
#include "spdlog/spdlog.h"

class TradeListener : public exchange_core::TradeEventListener
{
public:
  void onExecutionReport(const exchange_core::ExecutionReport &executionReport) override
  {
    spdlog::info("on execution report");
  }
  void onBalanceReport(const exchange_core::BalanceReport &balanceReport) override
  {
    spdlog::info("on balance report");
  }
};

TEST(trade, test1)
{
  exchange_phemex::Exchange exchange;

  exchange.GetConfig().SetParameter("apikey", "d5f11446-1fe4-4385-a39a-0a37acb52192");
  exchange.GetConfig().SetParameter("secretkey", "PKZMvdjxqLu_LdMNHlLlNS00qI2Zt4LRfsSg6GkFCTphZjc3NjFhYS00ZWNiLTQyNmQtYTRmNS04YTVjNmVkODU5Yjc");
  exchange.GetConfig().SetParameter("host", "api.huobi.pro");
  exchange.GetConfig().SetParameter("wshost", "api.huobi.pro");

  TradeListener listener;

  auto tradeService = exchange.GetTradeService();
  tradeService->RegisterListener(listener);

  tradeService->Connect();

  auto orders = tradeService->GetOpenOrder();

  for ( auto o: orders)
  {
    tradeService->CancelOrder(o);
  }

  tradeService->CancelAll();

  exchange_core::TradeOrder order;

  order.symbol = "BTCUSD";
  order.side = exchange_core::Side::BUY;
  order.price = 40000.01;
  order.quantity = 2;
  order.orderType = exchange_core::OrderType::LIMIT_POST_ONLY;
  order.timeInForce = exchange_core::TimeInForce::GTC;
  
//  tradeService->CancelOrder(order);
//  tradeService->CancelAll();

  long start = exchange_core::currentTimeInMilli();
  bool order_send = false;
  for(;;)
  {
    tradeService->RunOnce();
    long current = exchange_core::currentTimeInMilli();
    if ( current - start > 5000 && !order_send)
    {
      order.clientOrderId = std::to_string(exchange_core::get_current_ms_epoch());
      tradeService->PlaceOrder(order);
      order_send = true;

    }
  }
}

