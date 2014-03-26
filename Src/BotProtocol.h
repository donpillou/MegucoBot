
#pragma once

#include <nstd/Base.h>

class BotProtocol
{
public:
  enum MessageType
  {
    errorResponse,
    loginRequest,
    loginResponse,
    authRequest,
    authResponse,
    createSimSessionRequest,
    createSimSessionResponse,
    createSessionRequest,
    createSessionResponse,
    //simSessionMessage,
    //simSessionRemoveMessage,
    //sessionMessage,
    //sessionRemoveMessage,

    engineMessage,

    registerBotRequest,
    registerBotResponse,
  };

#pragma pack(push, 1)
  struct Header
  {
    uint32_t size; // including header
    uint64_t source; // client id
    uint64_t destination; // client id
    uint16_t messageType; // MessageType
  };

  struct ErrorResponse
  {
    uint16_t messageType;
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

  struct CreateSimSessionRequest
  {
    char_t name[33];
    char_t engine[33];
    double balanceBase;
    double balanceComm;
  };

  struct CreateSimSessionResponse
  {
    uint32_t id;
  };

  struct CreateSessionRequest
  {
    uint32_t simSessionId;
    double balanceBase;
    double balanceComm;
  };

  struct CreateSessionResponse
  {
    uint32_t id;
  };

  struct EngineMessage
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
