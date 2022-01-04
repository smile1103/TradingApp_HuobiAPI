#pragma once
#include <string>
#include <vector>
#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <chrono>
#include <sstream>

#include "shm.h"

namespace exchange_core
{
	enum class InstrumentType
	{
		STOCK,
		FUTURE,
		OPTIONS,
		CRYPTO
	};

	struct Instrument
	{
		std::string symbol;
		InstrumentType type;
		int instrument_id;
	};

	struct FeedTrade
	{
		int instrument_id;
		long timestamp;
		double price;
		double quantity;
		Side side;
		ExchangeEnum exchange;
	};

	struct FeedPriceLevel
	{
		double quantity;
		double price;
		int number_of_order;
	};

	struct OrderBook
	{
		ExchangeEnum exchange;
		int instrumentId;
		long timestamp;
		std::vector<FeedPriceLevel> asks;
		std::vector<FeedPriceLevel> bids;
	};

	class MarketDataEventListener
	{
	public:
		virtual void onOrderBook(const OrderBook &orderbook) = 0;
		virtual void onTrade(const FeedTrade &trade) = 0;
	};

	class MarketDataService
	{
	public:
		virtual void Connect() = 0;
		virtual void Subscribe(Instrument &instr) = 0;
		virtual void Unsubscribe(Instrument &instr) = 0;
		virtual void RegisterListener(MarketDataEventListener &listener) = 0;
		virtual void RunOnce() = 0;
	};

	std::string to_string(Side side)
	{
		if ( side == Side::BUY )
			return "buy";
		return "sell";
	};

	std::string to_string(ExecType exec)
	{
		switch (exec)
		{
			case ExecType::NEW:
				return "new";
			case ExecType::TRADE:
				return "trade";
			case ExecType::CANCELLED:
				return "canceled";
			case ExecType::REJECTED:
				return "rejected";
			case ExecType::EXPIRED:
				return "expired";
			case ExecType::FILLED:
				return "FILLED";
			case ExecType::PENDING_CANCEL:
				return "PENDING_CANCEL";
			case ExecType::PENDING_NEW:
				return "PENDING_NEW";
		}
		return "UNKNOWN";
	};

	std::string to_string(OrderStatus status)
	{
		switch (status)
		{
			case OrderStatus::NEW:
				return "NEW";
			case OrderStatus::CANCELLED:
				return "CANCELED";
			case OrderStatus::FILLED:
				return "FILLED";
			case OrderStatus::REJECTED:
				return "REJECTED";
			case OrderStatus::PENDING_NEW:
				return "PENDING_NEW";
			case OrderStatus::PARTIALLY_FILLED:
				return "PARTIALLY_FILLED";
			case OrderStatus::PENDING_CANCEL:
				return "PENDING_CANCEL";
		}
		return "UNKNOWN";
	};

	struct TradeOrder
	{
		std::string clientOrderId;
		std::string exchangeOrderId;
		double price;
		double quantity;
		Side side;
		std::string symbol;
		int instrumentId;
		OrderStatus orderStatus;
		OrderType orderType;
		double cumulateQuantity;
		double averagePrice;
		long createTime;
		long ackTime;
		long lastUpdateTime;
		TimeInForce timeInForce;

		std::string to_string()
		{
			std::ostringstream ss;
			ss << "Order:" << symbol << "," << exchange_core::to_string(side) << "," << price << "," << quantity << "," << clientOrderId;
			return ss.str();
		}
	};

	struct ExecutionReport
	{
		std::string orderID;
		std::string clOrderID;
		std::string origClOrderID;
		ExchangeEnum exchange;
		std::string symbol;
		OrderType orderType;
		Side side;
		double price;
		double quantity;
		OrderStatus orderStatus;
		double cumQuantity;
		double cumValue;
		double averagePrice;
		double lastExecPrice;
		double lastExecQuantity;
		long createTime;
		long transactionTime;
		long eventTime;
		ExecType execType;
		double fee;
		std::string feeCurrency;
		std::string tradeID;

		std::string to_string() const
		{
			std::ostringstream ss;
			ss << "ExecutionReport:" << symbol << "," << exchange_core::to_string(side) << "," << price << "," << quantity << "," << orderID << "," << clOrderID << "," << exchange_core::to_string(orderStatus) << "," << exchange_core::to_string(execType);
			return ss.str();
		}
	};

	struct BalanceReport
	{
		std::string currency;
		double totalBalance;
		double availableBalance;
		double frozenBalance;
	};

	class TradeEventListener
	{
	public:
		virtual void onExecutionReport(const ExecutionReport &executionReport) = 0;
		virtual void onBalanceReport(const BalanceReport &balanceReport) = 0;
	};

	class TradeService
	{
	public:
		virtual void Connect() = 0;
		virtual void RegisterListener(TradeEventListener &listener) = 0;
		virtual std::vector<TradeOrder> GetOpenOrder() = 0;
		virtual void PlaceOrder(const TradeOrder &order) = 0;
		virtual void CancelOrder(const TradeOrder &order) = 0;
		virtual void CancelAll() = 0;
		virtual void RunOnce() = 0;
	};

	class ExchangeConfig
	{
	public:
		std::string GetParameter(const std::string &name)
		{
			return mConfig[name];
		}

		void SetParameter(const std::string name, const std::string value)
		{
			mConfig[name] = value;
		}

	private:
		std::unordered_map<std::string, std::string> mConfig;
	};

	class Exchange
	{
	public:
		virtual ~Exchange();
		virtual MarketDataService *GetMarketDataService() = 0;
		virtual TradeService *GetTradeService() = 0;

		ExchangeConfig &GetConfig()
		{
			return mConfig;
		}

	protected:
		ExchangeConfig mConfig;
	};

	inline Exchange::~Exchange()
	{
	}

	unsigned long get_current_ms_epoch()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}

	class SecurityMaster
	{
	public:
		bool load(const std::string &file)
		{
			std::fstream fin;
			fin.open(file, std::ios::in);
			std::string line;

			while (std::getline(fin, line))
			{
				auto items = split(line, ',');
				Instrument instr;
				instr.symbol = items[1];
				instr.instrument_id = std::stoi(items[0]);
				mInstrumentMap[items[1]] = instr;
				//				spdlog::info(mInstrumentMap[items[1]].symbol);
			}

			return true;
		}

		Instrument getInstrument(const std::string &symbol)
		{
			return mInstrumentMap[symbol];
		}

	private:
		std::vector<std::string> split(const std::string &s, char delim)
		{
			std::stringstream ss(s);
			std::string item;
			std::vector<std::string> elems;
			while (std::getline(ss, item, delim))
			{
				elems.push_back(std::move(item)); // if C++11 (based on comment from @mchiasson)
			}
			return elems;
		}
		std::unordered_map<std::string, Instrument> mInstrumentMap;
	};

	exchange_core::ExchangeEnum toExchangeEnum(const std::string &name)
	{
		if (name == "binance")
		{
			return exchange_core::ExchangeEnum::BINANCE;
		}
		if (name == "crypto")
		{
			return exchange_core::ExchangeEnum::CRYPTO;
		}
		if (name == "binancefut")
		{
			return exchange_core::ExchangeEnum::BINANCEFUT;
		}
		if (name == "phemex")
		{
			return exchange_core::ExchangeEnum::PHEMEX;
		}
		spdlog::error("unrecognized exchange {}", name);
		return exchange_core::ExchangeEnum::CRYPTO;
	}

	long currentTimeInMilli()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
	// Get time stamp in microseconds.
	uint64_t currentTimeinMicro()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::
																																						now()
																																								.time_since_epoch())
											.count();
	}

	// Get time stamp in nanoseconds.
	uint64_t currentTimeInNano()
	{
		uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::
																																					 now()
																																							 .time_since_epoch())
											.count();
		return ns;
	}
}