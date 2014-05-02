
#pragma once

#include <nstd/Base.h>

class BotProtocol
{
public:
  enum MessageType
  {
    pingRequest,
    pingResponse, // a.k.a. pong
    loginRequest,
    loginResponse,
    authRequest,
    authResponse,
    registerBotRequest,
    registerBotResponse,
    registerMarketRequest,
    registerMarketResponse,
    updateEntity,
    removeEntity,
    controlEntity,
    controlEntityResponse,
    createEntity,
    requestEntities,
  };
  
  enum EntityType
  {
    error,
    session,
    botEngine,
    marketAdapter,
    sessionTransaction,
    sessionOrder,
    market,
    marketTransaction,
    marketOrder,
    marketBalance,
  };

#pragma pack(push, 1)
  struct Header
  {
    uint32_t size; // including header
    uint16_t messageType; // MessageType
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
    uint8_t simulation;
    double balanceBase;
    double balanceComm;
  };

  struct RegisterMarketRequest
  {
    uint32_t pid;
  };

  struct RegisterMarketResponse
  {
    char_t userName[33];
    char_t key[65];
    char_t secret[65];
  };

  struct Error : public Entity
  {
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

  struct MarketBalance : public Entity
  {
    double reservedUsd;
    double reservedBtc;
    double availableUsd;
    double availableBtc;
    double fee;
  };

  struct ControlSessionArgs : public Entity
  {
    enum Command
    {
      startSimulation,
      stop,
      select,
    };

    uint8_t cmd;
  };

  struct ControlSessionResponse : public Entity
  {
    uint8_t cmd;
    uint8_t success;
  };

  struct ControlMarketArgs : public Entity
  {
    enum Command
    {
      select,
      refreshTransactions,
      refreshOrders,
      refreshBalance,
    };

    uint8_t cmd;
  };

  struct ControlMarketResponse : public Entity
  {
    uint8_t cmd;
    uint8_t success;
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
