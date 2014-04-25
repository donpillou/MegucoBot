
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
    updateEntity,
    removeEntity,
    controlEntity,
    createEntity,
    requestEntities,
  };
  
  enum EntityType
  {
    error,
    session,
    engine,
    marketAdapter,
    transaction,
    order,
    market,
  };

#pragma pack(push, 1)
  struct Header
  {
    uint32_t size; // including header
    uint16_t messageType; // MessageType
    uint16_t entityType; // EntityType
    uint32_t entityId;
  };

  struct Error
  {
    char_t errorMessage[129];
  };

  struct LoginRequest
  {
    char_t username[33];
  };

  struct LoginResponse
  {
    byte_t userkey[32];
    byte_t loginkey[32];
  };

  struct AuthRequest
  {
    byte_t signature[32];
  };

  struct Session
  {
    enum State
    {
      stopped,
      active,
      simulating,
    };

    char_t name[33];
    uint32_t engineId;
    uint32_t marketId;
    uint8_t state;
  }; 

  struct Engine
  {
    char_t name[33];
  };

  struct MarketAdapter
  {
    char_t name[33];
    char_t currencyBase[33];
    char_t currencyComm[33];
  };

  struct Transaction
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

  struct Order
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

  struct Market
  {
    enum State
    {
      stopped,
      running,
    };

    char name[33];
    char currencyBase[33];
    char currencyComm[33];
    uint8_t state;
  };

  struct RegisterBotRequest
  {
    uint32_t pid;
  };
  
  struct RegisterBotResponse
  {
    uint8_t isSimulation;
    double balanceBase;
    double balanceComm;
  };

  struct CreateSessionArgs
  {
    char_t name[33];
    uint32_t engineId;
    uint32_t marketId;
    double balanceBase;
    double balanceComm;
  };

  struct ControlSessionArgs
  {
    enum Command
    {
      startSimulation,
      stop,
      select,
    };

    uint8_t cmd;
  };

  struct CreateTransactionArgs
  {
    uint8_t type; // see Transaction::Type
    double price;
    double amount;
    double fee;
  };

  struct CreateOrderArgs
  {
    uint8_t type; // see Order::Type
    double price;
    double amount;
    double fee;
  };
  
  struct CreateMarketArgs
  {
    uint32_t marketAdapterId;
    char_t username[33];
    char_t key[65];
    char_t secret[65];
  };

#pragma pack(pop)

};
