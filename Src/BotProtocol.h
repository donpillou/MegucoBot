
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
    createSessionRequest,
    createSessionResponse,
    registerBotRequest,
    registerBotResponse,
    updateEntity,
    removeEntity,
  };
  
  enum EntityType
  {
    error,
    session,
    engine,
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

  struct CreateSessionRequest
  {
    char_t name[33];
    char_t engine[33];
    double balanceBase;
    double balanceComm;
  };

  struct CreateSessionResponse
  {
    uint32_t id;
  };
  
  struct Session
  {
    char_t name[33];
    char_t engine[33];
  }; 

  struct Engine
  {
    char_t name[33];
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

#pragma pack(pop)

};
