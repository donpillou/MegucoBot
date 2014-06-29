
#if 0
#include "DataConnectionHandler.h"

void DataConnectionHandler::receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade)
{
  lastReceivedTradeId = trade.id;
  tradeHandler.add(trade, 0LL);

  if(simulation)
  {
    if(startTime == 0)
      startTime = trade.time;
    if(trade.time - startTime <= 45 * 60 * 1000)
      return; // wait for 45 minutes of trade data to be evaluated
    if(trade.flags & DataProtocol::syncFlag)
      broker.warning("sync");
  }
  else if(trade.flags & DataProtocol::replayedFlag)
    return;

  broker.handleTrade(trade);
  session.handle(trade, tradeHandler.values);
}
#endif
