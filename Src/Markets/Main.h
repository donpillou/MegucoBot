
#pragma once

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"
#include "Tools/Market.h"

#ifdef MARKET_BITSTAMPBTCUSD
#include "BitstampBtcUsd.h"
typedef BitstampBtcUsd MarketConnection;
#endif
#ifdef MARKET_MTGOXBTCUSD
#include "MtGoxBtcUsd.h"
typedef MtGoxBtcUsd MarketConnection;
#endif
#ifdef MARKET_HUOBIBTCCNY
#include "HuobiBtcCny.h"
typedef HuobiBtcCny MarketConnection;
#endif
#ifdef MARKET_BTCCHINABTCCNY
#include "BtcChinaBtcCny.h"
typedef BtcChinaBtcCny MarketConnection;
#endif
#ifdef MARKET_BITFINEXBTCUSD
#include "BitfinexBtcUsd.h"
typedef BitfinexBtcUsd MarketConnection;
#endif
#ifdef MARKET_BTCEBTCUSD
#include "BtceBtcUsd.h"
typedef BtceBtcUsd MarketConnection;
#endif
#ifdef MARKET_KRAKENBTCUSD
#include "KrakenBtcUsd.h"
typedef KrakenBtcUsd MarketConnection;
#endif

class Main : public ZlimdbConnection::Callback, public Market::Callback
{
public:
  bool_t connect();
  void_t process();

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity) {};
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {};
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId) {};
  virtual void_t controlEntity(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size) {};

public: // Market::Callback
  virtual bool_t receivedTrade(const Market::Trade& trade);
  virtual bool_t receivedTicker(const Market::Ticker& ticker);

private:
  ZlimdbConnection zlimdbConnection;
  MarketConnection marketConnection;
  uint32_t tradesTableId;
  uint32_t tickerTableId;
};
