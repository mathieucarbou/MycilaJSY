// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

float Mycila::JSY::Metrics::thdi(float phi) const {
  if (powerFactor == 0)
    return NAN;
  const float cosPhi = phi == 1 ? 1 : cos(phi);
  return sqrt((cosPhi * cosPhi) / (powerFactor * powerFactor) - 1);
}

float Mycila::JSY::Metrics::resistance() const { return current == 0 ? NAN : abs(activePower / (current * current)); }

float Mycila::JSY::Metrics::dimmedVoltage() const { return current == 0 ? NAN : abs(activePower / current); }

float Mycila::JSY::Metrics::nominalPower() const { return activePower == 0 ? NAN : abs(voltage * voltage * current * current / activePower); }

void Mycila::JSY::Metrics::clear() {
  voltage = NAN;
  current = NAN;
  activePower = NAN;
  reactivePower = NAN;
  apparentPower = NAN;
  powerFactor = NAN;
  activeEnergy = NAN;
  activeEnergyImported = NAN;
  activeEnergyReturned = NAN;
  reactiveEnergy = NAN;
  reactiveEnergyImported = NAN;
  reactiveEnergyReturned = NAN;
  apparentEnergy = NAN;
}

bool Mycila::JSY::Metrics::operator==(const Mycila::JSY::Metrics& other) const {
  return (isnanf(voltage) ? isnanf(other.voltage) : voltage == other.voltage) &&
         (isnanf(current) ? isnanf(other.current) : current == other.current) &&
         (isnanf(activePower) ? isnanf(other.activePower) : activePower == other.activePower) &&
         (isnanf(reactivePower) ? isnanf(other.reactivePower) : reactivePower == other.reactivePower) &&
         (isnanf(apparentPower) ? isnanf(other.apparentPower) : apparentPower == other.apparentPower) &&
         (isnanf(powerFactor) ? isnanf(other.powerFactor) : powerFactor == other.powerFactor) &&
         (isnanf(activeEnergy) ? isnanf(other.activeEnergy) : activeEnergy == other.activeEnergy) &&
         (isnanf(activeEnergyImported) ? isnanf(other.activeEnergyImported) : activeEnergyImported == other.activeEnergyImported) &&
         (isnanf(activeEnergyReturned) ? isnanf(other.activeEnergyReturned) : activeEnergyReturned == other.activeEnergyReturned) &&
         (isnanf(reactiveEnergy) ? isnanf(other.reactiveEnergy) : reactiveEnergy == other.reactiveEnergy) &&
         (isnanf(reactiveEnergyImported) ? isnanf(other.reactiveEnergyImported) : reactiveEnergyImported == other.reactiveEnergyImported) &&
         (isnanf(reactiveEnergyReturned) ? isnanf(other.reactiveEnergyReturned) : reactiveEnergyReturned == other.reactiveEnergyReturned) &&
         (isnanf(apparentEnergy) ? isnanf(other.apparentEnergy) : apparentEnergy == other.apparentEnergy);
}

Mycila::JSY::Metrics& Mycila::JSY::Metrics::operator+=(const Mycila::JSY::Metrics& other) {
  // voltage is not aggregated
  current += other.current;
  activePower += other.activePower;
  apparentPower += other.apparentPower;
  activeEnergy += other.activeEnergy;
  activeEnergyImported += other.activeEnergyImported;
  activeEnergyReturned += other.activeEnergyReturned;
  reactiveEnergy += other.reactiveEnergy;
  reactiveEnergyImported += other.reactiveEnergyImported;
  reactiveEnergyReturned += other.reactiveEnergyReturned;
  apparentEnergy += other.apparentEnergy;
  powerFactor = apparentPower == 0 ? NAN : abs(activePower / apparentPower);
  reactivePower = sqrt(apparentPower * apparentPower - activePower * activePower);
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
  if (!isnan(voltage))
    root["voltage"] = voltage;
  if (!isnan(current))
    root["current"] = current;
  if (!isnan(activePower))
    root["active_power"] = activePower;
  if (!isnan(reactivePower))
    root["reactive_power"] = reactivePower;
  if (!isnan(apparentPower))
    root["apparent_power"] = apparentPower;
  if (!isnan(powerFactor))
    root["power_factor"] = powerFactor;
  if (!isnan(activeEnergy))
    root["active_energy"] = activeEnergy;
  if (!isnan(apparentEnergy))
    root["apparent_energy"] = apparentEnergy;
  if (!isnan(activeEnergyImported))
    root["active_energy_imported"] = activeEnergyImported;
  if (!isnan(activeEnergyReturned))
    root["active_energy_returned"] = activeEnergyReturned;
  if (!isnan(reactiveEnergy))
    root["reactive_energy"] = reactiveEnergy;
  if (!isnan(reactiveEnergyImported))
    root["reactive_energy_imported"] = reactiveEnergyImported;
  if (!isnan(reactiveEnergyReturned))
    root["reactive_energy_returned"] = reactiveEnergyReturned;
  float r = resistance();
  float d = dimmedVoltage();
  float n = nominalPower();
  float t = thdi();
  if (!isnan(r))
    root["resistance"] = r;
  if (!isnan(d))
    root["dimmed_voltage"] = d;
  if (!isnan(n))
    root["nominal_power"] = n;
  if (!isnan(t))
    root["thdi"] = t;
}
#endif
