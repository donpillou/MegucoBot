

#include "Bot.h"
#include "BotProtocol.h"
#include "DataProtocol.h"

class Broker : public Bot::Broker
{
public:
  virtual void_t loadTransaction(const BotProtocol::Transaction& transaction) = 0;
  virtual void_t loadOrder(const BotProtocol::Order& order) = 0;
  virtual bool_t handleTrade(const DataProtocol::Trade& trade) = 0;
};

