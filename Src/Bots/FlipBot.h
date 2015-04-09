
#pragma once

#include "Tools/Bot.h"

class FlipBot : public Bot
{
public:

private:
  class Session : public Bot::Session
  {
  public:
    Session(Broker& broker);

  private:
    Broker& broker;

    virtual ~Session() {}

    void_t updateBalance();

    void_t checkBuy(const meguco_trade_entity& trade);
    void_t checkSell(const meguco_trade_entity& trade);

  private: // Bot::Session
    virtual void_t handleTrade(const meguco_trade_entity& trade, timestamp_t tradeAge);
    virtual void_t handleBuy(uint64_t orderId, const meguco_user_market_transaction_entity& transaction);
    virtual void_t handleSell(uint64_t orderId, const meguco_user_market_transaction_entity& transaction);
    virtual void_t handleBuyTimeout(uint64_t orderId);
    virtual void_t handleSellTimeout(uint64_t orderId);
    virtual void_t handlePropertyUpdate(const meguco_user_session_property_entity& property) {};
    virtual void_t handleAssetUpdate(const meguco_user_session_asset_entity& asset);
    virtual void_t handleAssetRemoval(const meguco_user_session_asset_entity& asset);
  };

public: // Bot
  virtual Session* createSession(Broker& broker) {return new Session(broker);};
  virtual timestamp_t getMaxTradeAge() const {return 0;}
};
