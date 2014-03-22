
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
    byte_t userkey[64];
    byte_t loginkey[64];
  };

  struct AuthRequest
  {
    byte_t signature[64];
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
    uint64_t id;
  };

  struct CreateSessionRequest
  {
    uint64_t simSessionId;
    double balanceBase;
    double balanceComm;
  };

  struct CreateSessionResponse
  {
    uint64_t id;
  };

#pragma pack(pop)

};
