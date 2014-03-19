
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

#pragma pack(pop)

};
