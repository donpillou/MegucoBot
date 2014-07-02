
#pragma once

#include <nstd/Base.h>

class BotProtocol
{
public:
  enum MessageType
  {
    errorResponse,
    pingRequest,
    pingResponse, // a.k.a. pong
    loginRequest,
    loginResponse,
    authRequest,
    authResponse,
    registerBotRequest,
    registerBotResponse,
    registerBotHandlerRequest,
    registerBotHandlerResponse,
    registerMarketRequest,
    registerMarketResponse,
    registerMarketHandlerRequest,
    registerMarketHandlerResponse,
    updateEntity,
    updateEntityResponse,
    removeEntity,
    removeEntityResponse,
    controlEntity,
    controlEntityResponse,
    createEntity,
    createEntityResponse,
    removeAllEntities,
  };
  
  enum EntityType
  {
    none,
    session,
    botEngine,
    marketAdapter,
    sessionTransaction,
    sessionOrder,
    sessionMarker,
    sessionLogMessage,
    sessionBalance,
    market,
    marketTransaction,
    marketOrder,
    marketBalance,
    sessionItem,
  };

#pragma pack(push, 4)
  struct Header
  {
    uint32_t size; // including header
    uint16_t messageType; // MessageType
    uint32_t requestId;
  };

  struct Entity
  {
    uint16_t entityType; // EntityType
    uint32_t entityId;
  };

  struct LoginRequest
  {
    char_t userName[33];
  };

  struct LoginResponse
  {
    byte_t userKey[32];
    byte_t loginKey[32];
  };

  struct AuthRequest
  {
    byte_t signature[32];
  };

  struct RegisterBotRequest
  {
    uint32_t pid;
  };

  struct RegisterBotResponse
  {
    uint32_t sessionId;
    uint32_t marketId;
  };

  struct RegisterBotHandlerRequest
  {
    uint32_t pid;
  };

  struct RegisterBotHandlerResponse
  {
    char_t marketAdapterName[33];
    uint8_t simulation;
  };

  struct RegisterMarketRequest
  {
    uint32_t pid;
  };

  struct RegisterMarketHandlerRequest
  {
    uint32_t pid;
  };

  struct RegisterMarketHandlerResponse
  {
    char_t userName[33];
    char_t key[65];
    char_t secret[65];
  };

  struct ErrorResponse : public Entity
  {
    uint16_t messageType;
    char_t errorMessage[129];
  };

  struct Session : public Entity
  {
    enum State
    {
      stopped,
      starting,
      running,
      simulating,
    };

    char_t name[33];
    uint32_t botEngineId;
    uint32_t marketId;
    uint8_t state;
    double balanceBase;
    double balanceComm;
  }; 

  struct BotEngine : public Entity
  {
    char_t name[33];
  };

  struct MarketAdapter : public Entity
  {
    char_t name[33];
    char_t currencyBase[33];
    char_t currencyComm[33];
  };

  struct Transaction : public Entity
  {
    enum Type
    {
      buy,
      sell
    };

    uint8_t type;
    int64_t date;
    double price;
    double amount;
    double fee;
  };

  struct Order : public Entity
  {
    enum Type
    {
      buy,
      sell,
    };

    uint8_t type;
    int64_t date;
    double price;
    double amount;
    double fee;
    int64_t timeout;
  };

  struct Marker : public Entity
  {
    enum Type
    {
      buy,
      sell,
      buyAttempt,
      sellAttempt,
    };

    uint8_t type;
    int64_t date;
  };

  struct Market : public Entity
  {
    enum State
    {
      stopped,
      starting,
      running,
    };

    uint32_t marketAdapterId;
    uint8_t state;
    char_t userName[33];
    char_t key[65];
    char_t secret[65];
  };

  struct Balance : public Entity
  {
    double reservedUsd; // usd in open orders
    double reservedBtc; // btc in open orders
    double availableUsd;
    double availableBtc;
    double fee;
  };

  struct SessionLogMessage : public Entity
  {
    int64_t date;
    char_t message[129];
  };

  struct SessionItem : public Entity
  {
    enum Type
    {
      buy,
      sell
    };

    enum State
    {
      waitBuy,
      buying,
      waitSell,
      selling,
    };

    uint8_t type;
    uint8_t state;
    int64_t date;
    double price;
    double amount;
    double profitablePrice;
    double flipPrice;
  };

  struct ControlSession : public Entity
  {
    enum Command
    {
      startSimulation,
      startLive,
      stop,
      select,
      requestTransactions,
      requestOrders,
      requestBalance,
      requestItems,
    };

    uint8_t cmd;
  };

  struct ControlSessionResponse : public Entity
  {
    uint8_t cmd;
  };

  struct ControlMarket : public Entity
  {
    enum Command
    {
      select,
      refreshTransactions,
      refreshOrders,
      refreshBalance,
      requestTransactions,
      requestOrders,
      requestBalance,
    };

    uint8_t cmd;
  };

  struct ControlMarketResponse : public Entity
  {
    uint8_t cmd;
  };

#pragma pack(pop)

  template<size_t N> static void_t setString(char_t(&str)[N], const String& value)
  {
    size_t size = value.length() + 1;
    if(size > N - 1)
      size = N - 1;
    Memory::copy(str, (const char_t*)value, size);
    str[N - 1] = '\0';
  }
  
  template<size_t N> static String getString(char_t(&str)[N])
  {
    str[N - 1] = '\0';
    String result;
    result.attach(str, String::length(str));
    return result;
  }
};
