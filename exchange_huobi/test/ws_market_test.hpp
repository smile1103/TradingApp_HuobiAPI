#include <gtest/gtest.h>

#include "config.hpp"
#include <thread>
#include <chrono>

#include "exchange-binance/WSMarketClinet.hpp"
#include "spdlog/spdlog.h"

using WSMarketClient = exchange_binance::WSMarketClient;

TEST(WSMarketClient, SubDepth)
{
    extern map<string, string> config;
    init_config();
    WSMarketClient ws;
    ws.SubDepth("btcusdt", "step0", [](const SubDepthResponse &data) {
        for (auto item : data.tick.asks)
        {
          spdlog::info("hello");
//            spdlog::info(*item.begin() << "/" << *(item.begin() + 1));
        }
    });
    ws.RunForever();
}