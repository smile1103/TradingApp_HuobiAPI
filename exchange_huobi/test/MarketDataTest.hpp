#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "exchange-phemex/exchange-phemex.h"
#include "spdlog/spdlog.h"


TEST(marketdata, test1)
{
  exchange_phemex::Exchange exchange;

  auto mkservice = exchange.GetMarketDataService();

  exchange_core::Instrument btcusdt;
  btcusdt.symbol = "BTC-USDT";
  mkservice->Subscribe(btcusdt);

  exchange_core::Instrument ethusdt;
  ethusdt.symbol = "ETH-USDT";
  mkservice->Subscribe(ethusdt);


  mkservice->Connect();



  for(;;)
  {
    mkservice->RunOnce();
  }

}