
#pragma once

#include <nstd/HashMap.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class Main : public ZlimdbConnection::Callback
{
public:
  ~Main();

  const String& getErrorString() const {return error;}

  void_t addMarket(const String& executable);

  bool_t connect();
  void_t disconnect();
  bool_t process();

private:
  struct Market
  {
    bool_t running;
  };

private:
  ZlimdbConnection connection;
  String error;
  uint32_t processesTableId;
  HashMap<String, Market> markets;
  HashMap<uint64_t, Market*> processes;

private:
  void_t addedProcess(uint64_t entityId, const String& command);
  void_t removedProcess(uint64_t entityId);

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId);
  virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size) {}
};
