
#pragma once

#include <nstd/Base.h>
#include <nstd/Variant.h>

#include "BotProtocol.h"

class Order
{
public:
  Order(uint32_t id, double price, double amount, double fee, BotProtocol::Order::Type type);
  Order(const Variant& variant);
  void_t toVariant(Variant& variant);

  uint32_t getId() const {return id;}
  double getPrice() const {return price;}
  double getAmount() const {return amount;}
  double getFee() const {return fee;}
  BotProtocol::Order::Type getType() const {return type;}
  timestamp_t getDate() const {return date;}

private:
  uint32_t id;
  double price;
  double amount;
  double fee;
  BotProtocol::Order::Type type;
  timestamp_t date;
};
