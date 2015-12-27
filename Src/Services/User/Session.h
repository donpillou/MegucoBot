
#pragma once

#include <nstd/Buffer.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class User;

class Session
{
public:
  Session(User & user, uint64_t sessionId)
    : user(user), sessionId(sessionId), sessionTableId(0),
    ordersTableId(0), transactionsTableId(0), assetsTableId(0), logTableId(0), propertiesTableId(0), markersTableId(0) {}

  User & getUser() {return user;}
  uint64_t getSessionId() const {return sessionId;}

  void_t setSessionTableId(uint32_t tableId) {sessionTableId = tableId;}
  uint32_t getSessionTableId() const {return sessionTableId;}
  void_t setOrdersTableId(uint32_t tableId) {ordersTableId = tableId;}
  uint32_t getOrdersTableId() const {return ordersTableId;}
  void_t setTransactionsTableId(uint32_t tableId) {transactionsTableId = tableId;}
  uint32_t getTransactionsTableId() const {return transactionsTableId;}
  void_t setAssetsTableId(uint32_t tableId) {assetsTableId = tableId;}
  uint32_t getAssetsTableId() const {return assetsTableId;}
  void_t setLogTableId(uint32_t tableId) {logTableId = tableId;}
  uint32_t getLogTableId() const {return logTableId;}
  void_t setPropertiesTableId(uint32_t tableId) {propertiesTableId = tableId;}
  uint32_t getPropertiesTableId() const {return propertiesTableId;}
  void_t setMarkersTableId(uint32_t tableId) {markersTableId = tableId;}
  uint32_t getMarkersTableId() const {return markersTableId;}

  meguco_user_session_state getState() const {return (meguco_user_session_state)((const meguco_user_session_entity*)(const byte_t*)sessionEntity)->state;}
  void_t setState(meguco_user_session_state state) {((meguco_user_session_entity*)(byte_t*)sessionEntity)->state = state;}
  meguco_user_session_mode getMode() const {return (meguco_user_session_mode)((const meguco_user_session_entity*)(const byte_t*)sessionEntity)->mode;}
  void_t setMode(meguco_user_session_mode mode) {((meguco_user_session_entity*)(byte_t*)sessionEntity)->mode = mode;}
  bool_t hasEntity() const {return !sessionEntity.isEmpty();}
  void_t setEntity(const meguco_user_session_entity& entity) {sessionEntity.assign((const byte_t*)&entity, entity.entity.size);}
  const zlimdb_entity& getEntity() const {return ((const meguco_user_session_entity*)(const byte_t*)sessionEntity)->entity;}

  const String& getCommand() const {return command;}
  void_t setCommand(const String& command) {this->command = command;}

private:
  User & user;
  uint64_t sessionId;
  uint32_t sessionTableId;
  uint32_t ordersTableId;
  uint32_t transactionsTableId;
  uint32_t assetsTableId;
  uint32_t logTableId;
  uint32_t propertiesTableId;
  uint32_t markersTableId;
  Buffer sessionEntity;
  String command;
};
