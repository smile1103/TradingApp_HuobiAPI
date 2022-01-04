#pragma once
#include "exchange-core/exchange-core.h"
#include <libwebsockets.h>

#include "TradeService.hpp"
#include "MarketDataService.hpp"

namespace exchange_phemex {

class Exchange : public exchange_core::Exchange
{
public:
  Exchange():
    mTradeService(mConfig)
  {

  }
  virtual ~Exchange()
  {

  }
  virtual exchange_core::MarketDataService * GetMarketDataService()
  {
    return (exchange_core::MarketDataService *)&mMarketDataService;
  }

  virtual exchange_core::TradeService * GetTradeService()
  {
    return (exchange_core::TradeService *)&mTradeService;
  }
private:
  MarketDataService mMarketDataService;
  TradeService  mTradeService;
};

}
