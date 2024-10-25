// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

void Mycila::JSY::Data::clear() {
  address = MYCILA_JSY_ADDRESS_UNKNOWN;
  model = MYCILA_JSY_MK_UNKNOWN;
  frequency = 0;
  _metrics[0].clear();
  _metrics[1].clear();
  _metrics[2].clear();
  _aggregate.clear();
}

bool Mycila::JSY::Data::operator==(const Mycila::JSY::Data& other) const {
  return address == other.address &&
         model == other.model &&
         frequency == other.frequency &&
         _metrics[0] == other._metrics[0] &&
         _metrics[1] == other._metrics[1] &&
         _metrics[2] == other._metrics[2] &&
         _aggregate == other._aggregate;
  ;
}

void Mycila::JSY::Data::operator=(const Mycila::JSY::Data& other) {
  address = other.address;
  model = other.model;
  frequency = other.frequency;
  _metrics[0] = other._metrics[0];
  _metrics[1] = other._metrics[1];
  _metrics[2] = other._metrics[2];
  _aggregate = other._aggregate;
}

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::Data::toJson(const JsonObject& root) const {
  root["address"] = address;
  root["model"] = Mycila::JSY::getModelName(model);
  root["frequency"] = frequency;

  switch (model) {
    {
      case MYCILA_JSY_MK_163:
        _metrics[0].toJson(root);
        break;

      case MYCILA_JSY_MK_194:
        _metrics[0].toJson(root["channel1"].to<JsonObject>());
        _metrics[1].toJson(root["channel2"].to<JsonObject>());
        _aggregate.toJson(root["aggregate"].to<JsonObject>());
        break;

      case MYCILA_JSY_MK_333:
        _metrics[0].toJson(root["phaseA"].to<JsonObject>());
        _metrics[1].toJson(root["phaseB"].to<JsonObject>());
        _metrics[2].toJson(root["phaseC"].to<JsonObject>());
        _aggregate.toJson(root["aggregate"].to<JsonObject>());
        break;

      default:
        break;
    }
  }
}
#endif
