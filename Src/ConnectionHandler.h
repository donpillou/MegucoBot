
#pragma once

#include "Tools/ZlimdbConnection.h"

class ConnectionHandler : public ZlimdbConnection::Callback
{
public:
  ConnectionHandler() {}
  ~ConnectionHandler() {}

  const String& getErrorString() const {return error;}

  void_t addMarketAdapter(const String& name, const String& executable);
  void_t addBotEngine(const String& name, const String& executable);

  bool_t connect();
  bool_t process();

private:
  ZlimdbConnection connection;
  String error;

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t removedEntity(uint32_t tableId, const zlimdb_entity& entity);
};
