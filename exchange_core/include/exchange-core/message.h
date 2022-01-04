#pragma once
#include <cstdint>

namespace exchange_core
{
  enum class ExecType
  {
    PENDING_NEW,
    NEW,
    CANCELLED,
    REJECTED,
    TRADE,
    EXPIRED, 
    FILLED,
    PENDING_CANCEL
  };

  enum class OrderType
  {
    LIMIT,
    MARKET,
    LIMIT_POST_ONLY
  };

  enum class Side
  {
    BUY = 0,
    SELL,
  };

  enum class TimeInForce
  {
    IOC,
    GTC
  };

  enum class OrderStatus
  {
    PENDING_NEW,
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    PENDING_CANCEL,
    CANCELLED,
    REJECTED,
  };

  enum class ExchangeEnum
  {
    BINANCE,
    CRYPTO,
    HUOBI,
    BINANCEFUT,
    PHEMEX, 
    BYBIT, 
    ITBIT
  };

  struct MessageHeader
  {
    uint16_t message_type;
    uint16_t message_size;
    char session[8];
    long seq;
  };

#define MAX_MESSAGE_SIZE 1024

  struct Message : MessageHeader
  {
    uint8_t data[MAX_MESSAGE_SIZE - sizeof(MessageHeader)];
  };

  enum MessageType
  {
    Trade = 1,
    BBO,
    LOGIN,
    LOGOFF,
    NEW_ORDER,
    CANCEL_ORDER,
    CANCEL_ALL_ORDER,
    GET_OPEN_ORDER,
    LOGIN_ACCEPT,
    LOGOFF_ACCEPT,
    EXECUTION_REPORT,
    BALANCE_REPORT,
    HEARTBEAT, 
    EECHO,
    JSON,
  };

  struct TradeMessage : MessageHeader
  {
    ExchangeEnum exchange;
    int instrument_id;
    double quantity;
    double price;
    long timestamp;
    Side side;

    TradeMessage()
    {
      message_type = MessageType::Trade;
      message_size = sizeof(TradeMessage);
    }
  };

  struct BBOMessage : MessageHeader
  {
    ExchangeEnum exchange;
    int instrument_id;
    double bid_quantity;
    double bid_price;
    double ask_quantity;
    double ask_price;
    long timestamp;

    BBOMessage()
    {
      message_type = MessageType::BBO;
      message_size = sizeof(BBOMessage);
    }
  };

  struct LoginMessage : MessageHeader
  {
    char session[16];
    char password[16];
    long timestamp;

    LoginMessage()
    {
      message_type = MessageType::LOGIN;
      message_size = sizeof(LoginMessage);
    }
  };

  struct LogOffMessage : MessageHeader
  {
    char session[8];
    long timestamp;

    LogOffMessage()
    {
      message_type = MessageType::LOGOFF;
      message_size = sizeof(LogOffMessage);
    }
  };

  struct NewOrderMessage : MessageHeader
  {
    long timestamp;
    char symbol[16];
    Side side;
    OrderType orderType;
    TimeInForce timeInForce;
    double price;
    double quantity;
    char clientOrderID[32];

    NewOrderMessage()
    {
      message_type = MessageType::NEW_ORDER;
      message_size = sizeof(NewOrderMessage);
    }
  };

  struct CancelOrderMessage : MessageHeader
  {
    long timestamp;
    char symbol[16];
    char orderID[64];
    char clientOrderID[32];

    CancelOrderMessage()
    {
      message_type = MessageType::CANCEL_ORDER;
      message_size = sizeof(CancelOrderMessage);
    }
  };

  struct CancelAllOrderMessage : MessageHeader
  {
    long timestamp;
    char symbol[16];

    CancelAllOrderMessage()
    {
      message_type = MessageType::CANCEL_ALL_ORDER;
      message_size = sizeof(CancelAllOrderMessage);
    }
  };

  struct GetOpenOrderMessage : MessageHeader
  {
    long timestamp;
    char symbol[16]{0x20};

    GetOpenOrderMessage()
    {
      message_type = MessageType::GET_OPEN_ORDER;
      message_size = sizeof(GetOpenOrderMessage);
    }
  };

  // outbound

  struct LoginAcceptMessage : MessageHeader
  {
    long timestamp;

    LoginAcceptMessage()
    {
      message_type = MessageType::LOGIN_ACCEPT;
      message_size = sizeof(LoginAcceptMessage);
    }
  };

  struct ExecutionReportMessage : MessageHeader
  {
    long timestamp;

    ExecutionReportMessage()
    {
      message_type = MessageType::EXECUTION_REPORT;
      message_size = sizeof(ExecutionReportMessage);
    }

    char orderID[64]{0};
    char clOrderID[32]{0};
    char origClOrderID[32]{0};
    char symbol[16]{0};
    char feeCurrency[8]{0};
    char tradeID[32]{0};

    ExecType execType;
    OrderType orderType;
    ExchangeEnum exchange;
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
    double fee;
  };

  struct BalanceReportMessage : MessageHeader
  {
    long timestamp;

    BalanceReportMessage()
    {
      message_type = MessageType::BALANCE_REPORT;
      message_size = sizeof(ExecutionReportMessage);
    }

    char currency[8]{0};
    double total;
    double available;
    double frozen;
  };

  struct EchoMessage : MessageHeader
  {
    long timestamp;

    EchoMessage()
    {
      message_type = MessageType::EECHO;
      message_size = sizeof(EchoMessage);
    }
  };

  struct JsonMessage : MessageHeader
  {
    long timestamp;

    JsonMessage()
    {
      message_type = MessageType::JSON;
      message_size = sizeof(JsonMessage);
    }

    char json_str[MAX_MESSAGE_SIZE - sizeof(MessageHeader) - 128];
  };
}