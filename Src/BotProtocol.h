
#pragma once

#include <nstd/Base.h>

class BotProtocol
{
public:
  enum MessageType
  {
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
  };
  
  enum EntityType
  {
    error,
    session,
    engine,
    market,
    transaction,
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
      inactive,
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

  struct Market
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

    int64_t date;
    double price;
    double amount;
    double fee;
    uint8_t type;
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
    double price;
    double amount;
    double fee;
    uint8_t type; // see Transaction::Type
  };

#pragma pack(pop)

};
