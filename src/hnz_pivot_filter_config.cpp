/*
 * FledgePower HNZ <-> pivot filter plugin.
 *
 * Copyright (c) 2022, RTE (https://www.rte-france.com)
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Michael Zillgith (michael.zillgith at mz-automation.de)
 * 
 */

#include <rapidjson/error/en.h>

#include "hnz_pivot_utility.hpp"
#include "hnz_pivot_filter_config.hpp"


HNZPivotDataPoint::HNZPivotDataPoint(const std::string& label, const std::string& pivotId, const std::string& pivotType,
                                    const std::string& typeIdStr, unsigned int address):
    m_label(label), m_pivotId(pivotId), m_pivotType(pivotType), m_typeIdStr(typeIdStr), m_address(address)
{}

HNZPivotDataPoint::~HNZPivotDataPoint()
{}

HNZPivotConfig::HNZPivotConfig()
{}

HNZPivotConfig::~HNZPivotConfig()
{}

void HNZPivotConfig::importExchangeConfig(const std::string& exchangeConfig)
{
  m_exchange_data_is_complete = false;
  bool is_complete = true;

  Document document;
  if (document.Parse(const_cast<char *>(exchangeConfig.c_str())).HasParseError()) {
    PivotUtility::log_fatal("Parsing error in exchanged_data json, offset " +
                               std::to_string(static_cast<unsigned>(document.GetErrorOffset())) +
                               " " +
                               GetParseError_En(document.GetParseError()));
    return;
  }
  if (!document.IsObject()) return;

  if (!m_check_object(document, JSON_EXCHANGED_DATA_NAME)) return;

  const Value &info = document[JSON_EXCHANGED_DATA_NAME];

  is_complete &= m_check_string(info, JSON_NAME);
  is_complete &= m_check_string(info, JSON_VERSION);

  if (!m_check_array(info, DATAPOINTS)) return;

  for (const Value &msg : info[DATAPOINTS].GetArray()) {
    if (!msg.IsObject()) return;

    std::string label;
    std::string pivotId;
    std::string pivotType;

    is_complete &= m_retrieve(msg, LABEL, &label);
    is_complete &= m_retrieve(msg, PIVOT_ID, &pivotId);
    is_complete &= m_retrieve(msg, PIVOT_TYPE, &pivotType);

    if (!m_check_array(msg, PROTOCOLS)) continue;
  
    for (const Value &protocol : msg[PROTOCOLS].GetArray()) {
      if (!protocol.IsObject()) return;

      std::string protocol_name;

      is_complete &= m_retrieve(protocol, JSON_NAME, &protocol_name);

      if (protocol_name != HNZ_NAME) continue;

      std::string address;
      std::string msg_code;

      is_complete &= m_retrieve(protocol, MESSAGE_ADDRESS, &address);
      is_complete &= m_retrieve(protocol, MESSAGE_CODE, &msg_code);
      
      unsigned long tmp = std::stoul(address);
      unsigned int msg_address = 0;
      // Check if number is in range for unsigned int
      if (tmp > static_cast<unsigned int>(-1)) {
        is_complete = false;
        PivotUtility::log_error("Error with the field %s, the value is out of range for unsigned integer: %ld", MESSAGE_ADDRESS, tmp);
      } else {
        msg_address = static_cast<unsigned int>(tmp);
      }
      auto newDp = std::make_shared<HNZPivotDataPoint>(label, pivotId, pivotType, msg_code, msg_address);
      m_exchangeDefinitions[pivotId] = newDp;
      m_pivotIdLookup[m_getLookupHash(msg_code, msg_address)] = pivotId;
    }
  }

  m_exchange_data_is_complete = is_complete;
}

bool HNZPivotConfig::m_check_string(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    std::string s = key;
    PivotUtility::log_error(
        "Error with the field " + s +
        ", the value does not exist or is not a std::string.");
    return false;
  }
  return true;
}

bool HNZPivotConfig::m_check_array(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsArray()) {
    std::string s = key;
    PivotUtility::log_error("The array " + s +
                               " is required but not found.");
    return false;
  }
  return true;
}

bool HNZPivotConfig::m_check_object(const Value &json, const char *key) {
  if (!json.HasMember(key) || !json[key].IsObject()) {
    std::string s = key;
    PivotUtility::log_error("The object " + s +
                               " is required but not found.");
    return false;
  }
  return true;
}

bool HNZPivotConfig::m_retrieve(const Value &json, const char *key,
                         unsigned int *target) {
  if (!json.HasMember(key) || !json[key].IsUint()) {
    std::string s = key;
    PivotUtility::log_error(
        "Error with the field " + s +
        ", the value does not exist or is not an unsigned integer.");
    return false;
  }
  *target = json[key].GetUint();
  return true;
}

bool HNZPivotConfig::m_retrieve(const Value &json, const char *key,
                         unsigned int *target, unsigned int def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsUint()) {
      std::string s = key;
      PivotUtility::log_error("Error with the field " + s +
                                 ", the value is not an unsigned integer.");
      return false;
    }
    *target = json[key].GetUint();
  }
  return true;
}

bool HNZPivotConfig::m_retrieve(const Value &json, const char *key, std::string *target) {
  if (!json.HasMember(key) || !json[key].IsString()) {
    std::string s = key;
    PivotUtility::log_error(
        "Error with the field " + s +
        ", the value does not exist or is not a std::string.");
    return false;
  }
  *target = json[key].GetString();
  return true;
}

bool HNZPivotConfig::m_retrieve(const Value &json, const char *key, std::string *target,
                         std::string def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsString()) {
      std::string s = key;
      PivotUtility::log_error("Error with the field " + s +
                                 ", the value is not a std::string.");
      return false;
    }
    *target = json[key].GetString();
  }
  return true;
}

bool HNZPivotConfig::m_retrieve(const Value &json, const char *key,
                         long long int *target, long long int def) {
  if (!json.HasMember(key)) {
    *target = def;
  } else {
    if (!json[key].IsInt64()) {
      std::string s = key;
      PivotUtility::log_error("Error with the field " + s +
                                 ", the value is not a long long integer.");
      return false;
    }
    *target = json[key].GetInt64();
  }
  return true;
}

std::string HNZPivotConfig::findPivotId(const std::string& typeIdStr, unsigned int address) const {
    const std::string& lookupHash = m_getLookupHash(typeIdStr, address);
    if (m_pivotIdLookup.count(lookupHash) == 0){
        return "";
    }
    return m_pivotIdLookup.at(lookupHash);
}

const std::string& HNZPivotConfig::getPluginName() {
  static std::string pluginName(FILTER_NAME);
  return pluginName;
}

std::string HNZPivotConfig::m_getLookupHash(const std::string& typeIdStr, unsigned int address) const {
    return typeIdStr + "-" + std::to_string(address);
}