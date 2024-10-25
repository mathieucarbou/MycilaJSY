// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

void Mycila::JSY::Metrics::clear() {
  voltage = 0;
  current = 0;
  activePower = 0;
  reactivePower = 0;
  apparentPower = 0;
  powerFactor = 0;
  activeEnergy = 0;
  activeEnergyImported = 0;
  activeEnergyReturned = 0;
  reactiveEnergy = 0;
  reactiveEnergyImported = 0;
  reactiveEnergyReturned = 0;
  apparentEnergy = 0;
}

bool Mycila::JSY::Metrics::operator==(const Mycila::JSY::Metrics& other) const {
  return voltage == other.voltage &&
         current == other.current &&
         activePower == other.activePower &&
         reactivePower == other.reactivePower &&
         apparentPower == other.apparentPower &&
         powerFactor == other.powerFactor &&
         activeEnergy == other.activeEnergy &&
         activeEnergyImported == other.activeEnergyImported &&
         activeEnergyReturned == other.activeEnergyReturned &&
         reactiveEnergy == other.reactiveEnergy &&
         reactiveEnergyImported == other.reactiveEnergyImported &&
         reactiveEnergyReturned == other.reactiveEnergyReturned &&
         apparentEnergy == other.apparentEnergy;
}

Mycila::JSY::Metrics& Mycila::JSY::Metrics::operator+=(const Mycila::JSY::Metrics& other) {
  voltage = (voltage + other.voltage) / 2;
  current += other.current;
  activePower += other.activePower;
  reactivePower += other.reactivePower;
  apparentPower += other.apparentPower;
  powerFactor += other.powerFactor;
  activeEnergy += other.activeEnergy;
  activeEnergyImported += other.activeEnergyImported;
  activeEnergyReturned += other.activeEnergyReturned;
  reactiveEnergy += other.reactiveEnergy;
  reactiveEnergyImported += other.reactiveEnergyImported;
  reactiveEnergyReturned += other.reactiveEnergyReturned;
  apparentEnergy += other.apparentEnergy;
  return *this;
}

void Mycila::JSY::Metrics::operator=(const Metrics& other) {
  voltage = other.voltage;
  current = other.current;
  activePower = other.activePower;
  reactivePower = other.reactivePower;
  apparentPower = other.apparentPower;
  powerFactor = other.powerFactor;
  activeEnergy = other.activeEnergy;
  activeEnergyImported = other.activeEnergyImported;
  activeEnergyReturned = other.activeEnergyReturned;
  reactiveEnergy = other.reactiveEnergy;
  reactiveEnergyImported = other.reactiveEnergyImported;
  reactiveEnergyReturned = other.reactiveEnergyReturned;
  apparentEnergy = other.apparentEnergy;
}

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::Metrics::toJson(const JsonObject& root) const {
  root["voltage"] = voltage;
  root["current"] = current;
  root["active_power"] = activePower;
  root["reactive_power"] = reactivePower;
  root["apparent_power"] = apparentPower;
  root["power_factor"] = powerFactor;
  root["active_energy"] = activeEnergy;
  root["active_energy_imported"] = activeEnergyImported;
  root["active_energy_returned"] = activeEnergyReturned;
  root["reactive_energy"] = reactiveEnergy;
  root["reactive_energy_imported"] = reactiveEnergyImported;
  root["reactive_energy_returned"] = reactiveEnergyReturned;
  root["apparent_energy"] = apparentEnergy;
  root["resistance"] = resistance();
  root["dimmed_voltage"] = dimmedVoltage();
  root["nominal_power"] = nominalPower();
  root["thdi"] = thdi();
}
#endif
