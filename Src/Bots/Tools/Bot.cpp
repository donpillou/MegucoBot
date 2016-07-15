
#include "Bot.h"

Bot::Order::Order(const meguco_user_broker_order_entity& entity)
{
  id = entity.entity.id;
  time = entity.entity.time;
  price = entity.price;
  amount = entity.amount;
  total = entity.total;
  raw_id = entity.raw_id;
  timeout = entity.timeout;
  type = entity.type;
  state = entity.state;
}

Bot::Order::operator meguco_user_broker_order_entity() const
{
  meguco_user_broker_order_entity entity;
  entity.entity.id = id;
  entity.entity.time = time;
  entity.entity.size = sizeof(meguco_user_broker_order_entity);
  entity.price = price;
  entity.amount = amount;
  entity.total = total;
  entity.raw_id = raw_id;
  entity.timeout = timeout;
  entity.type = type;
  entity.state = state;
  return entity;
}

Bot::Asset::Asset(const meguco_user_session_asset_entity& entity)
{
  id = entity.entity.id;
  time = entity.entity.time;
  price = entity.price;
  invest_comm = entity.invest_comm;
  invest_base = entity.invest_base;
  balance_comm = entity.balance_comm;
  balance_base = entity.balance_base;
  profitable_price = entity.profitable_price;
  flip_price = entity.flip_price;
  order_id = entity.order_id;
  last_transaction_time = entity.last_transaction_time;
  type = entity.type;
  state = entity.state;
}

Bot::Asset::operator meguco_user_session_asset_entity() const
{
  meguco_user_session_asset_entity entity;
  entity.entity.id = id;
  entity.entity.time = time;
  entity.entity.size = sizeof(meguco_user_session_asset_entity);
  entity.price = price;
  entity.invest_comm = invest_comm;
  entity.invest_base = invest_base;
  entity.balance_comm = balance_comm;
  entity.balance_base = balance_base;
  entity.profitable_price = profitable_price;
  entity.flip_price = flip_price;
  entity.order_id = order_id;
  entity.last_transaction_time = last_transaction_time;
  entity.type = type;
  entity.state = state;
  return entity;
}

Bot::Property::Property(const meguco_user_session_property_entity& entity, const String& name, const String& value, const String& unit)
{
  id = entity.entity.id;
  time = entity.entity.time;
  flags = entity.flags;
  type = entity.type;
  this->name = name;
  this->value = value;
  this->unit = unit;
}

Bot::Property::operator meguco_user_session_property_entity() const
{
  meguco_user_session_property_entity entity;
  entity.entity.id = id;
  entity.entity.time = time;
  entity.entity.size = sizeof(meguco_user_session_property_entity);
  entity.flags = flags;
  entity.type = type;
  return entity;
}

Bot::Transaction::Transaction(const meguco_user_broker_transaction_entity& entity)
{
  id = entity.entity.id;
  time = entity.entity.time;
  price = entity.price;
  amount = entity.amount;
  total = entity.total;
  raw_id = entity.raw_id;
  type = entity.type;
}

Bot::Transaction::operator meguco_user_broker_transaction_entity() const
{
  meguco_user_broker_transaction_entity entity;
  entity.entity.id = id;
  entity.entity.time = time;
  entity.entity.size = sizeof(meguco_user_broker_transaction_entity);
  entity.price = price;
  entity.amount = amount;
  entity.total = total;
  entity.raw_id = raw_id;
  entity.type = type;
  return entity;
}

Bot::Trade::Trade(const meguco_trade_entity& entity)
{
  id = entity.entity.id;
  time = entity.entity.time;
  price = entity.price;
  amount = entity.amount;
  flags = entity.flags;
}

Bot::Marker::operator meguco_user_session_marker_entity() const
{
  meguco_user_session_marker_entity entity;
  entity.entity.id = id;
  entity.entity.time = time;
  entity.entity.size = sizeof(meguco_user_session_marker_entity);
  entity.type = type;
  return entity;
}
