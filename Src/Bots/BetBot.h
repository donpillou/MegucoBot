
#pragma once

#include "Tools/Bot.h"

class BetBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    Broker& broker;
    int buyState;
    int sellState;
    double maxBuyPrice;
    double minSellPrice;
    double buyIncline;
    double sellIncline;

    virtual ~Session() {}

    void_t checkBuy(const DataProtocol::Trade& trade, const Values& values);
    void_t checkSell(const DataProtocol::Trade& trade, const Values& values);

  private: // Bot::Session
    virtual void_t handle(const DataProtocol::Trade& trade, const Values& values);
    virtual void_t handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction);
    virtual void_t handleBuyTimeout(uint32_t orderId);
    virtual void_t handleSellTimeout(uint32_t orderId);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
};
