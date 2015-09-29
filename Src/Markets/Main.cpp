
#include <nstd/Log.h>
#include <nstd/Thread.h>
#include <nstd/Directory.h>
#include <nstd/Error.h>
#include <nstd/Process.h>

#include "Main.h"

int_t main(int_t argc, char_t* argv[])
{
  Log::setFormat("%P> %m");

  Main main;
  for(;; Thread::sleep(10 * 1000))
  {
    if(!main.connect())
      continue;
    main.process();
  }
  return 0;
}

bool_t Main::connect()
{
  const String& channelName = marketConnection.getChannelName();

  if(!zlimdbConnection.isOpen())
  {
    Log::infof("Connecting to zlimdb server...");
    if(!zlimdbConnection.connect(*this))
      return Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)zlimdbConnection.getErrorString()), false;
    if(!zlimdbConnection.createTable(String("markets/") + channelName + "/trades", tradesTableId))
      return Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)zlimdbConnection.getErrorString()), false;
    if(!zlimdbConnection.createTable(String("markets/") + channelName + "/ticker", tickerTableId))
      return Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)zlimdbConnection.getErrorString()), false;

    Log::infof("Connected to zlimdb server.");
  }

  if(!marketConnection.isOpen())
  {
    Log::infof("Connecting to %s...", (const char_t*)channelName);
    if(!marketConnection.connect())
      return Log::errorf("Could not connect to %s: %s", (const char_t*)channelName, (const char_t*)marketConnection.getErrorString()), false;
    Log::infof("Connected to %s.", (const char_t*)channelName);
  }

  return true;
}

void_t Main::process()
{
  for(;;)
    if(!marketConnection.process(*this))
      break;

  if(!zlimdbConnection.isOpen())
    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)zlimdbConnection.getErrorString());
  if(!marketConnection.isOpen())
    Log::errorf("Lost connection to %s: %s", (const char_t*)marketConnection.getChannelName(), (const char_t*)marketConnection.getErrorString());
  marketConnection.close(); // reconnect to reload the trade history
}

bool_t Main::receivedTrade(const Market::Trade& trade)
{
  meguco_trade_entity tradeEntity;
  tradeEntity.entity.id = trade.id;
  tradeEntity.entity.time = trade.time;
  tradeEntity.entity.size = sizeof(tradeEntity);
  tradeEntity.amount = trade.amount;
  tradeEntity.price = trade.price;
  tradeEntity.flags = trade.flags;
  uint64_t id;
  if(!zlimdbConnection.add(tradesTableId, tradeEntity.entity, id, true))
    return false;
  return true;
}

bool_t Main::receivedTicker(const Market::Ticker& ticker)
{
  meguco_ticker_entity tickerEntity;
  tickerEntity.entity.id = 0;
  tickerEntity.entity.time = ticker.time;
  tickerEntity.entity.size = sizeof(tickerEntity);
  tickerEntity.ask = ticker.ask;
  tickerEntity.bid = ticker.bid;
  uint64_t id;
  if(!zlimdbConnection.add(tickerTableId, tickerEntity.entity, id))
    return false;
  return true;
}
