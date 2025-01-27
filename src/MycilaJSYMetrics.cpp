// SPDX-License-Identifier: MIT
/*
 * Copyright (C) 2023-2024 Mathieu Carbou
 */
#include "MycilaJSY.h"

float Mycila::JSY::Metrics::thdi(float phi) const {
  if (powerFactor == 0)
    return NAN;
  const float cosPhi = phi == 0 ? 1 : std::cos(phi);
  return std::sqrt((cosPhi * cosPhi) / (powerFactor * powerFactor) - 1);
}

float Mycila::JSY::Metrics::resistance() const { return current == 0 ? NAN : std::abs(activePower / (current * current)); }

float Mycila::JSY::Metrics::dimmedVoltage() const { return current == 0 ? NAN : std::abs(activePower / current); }

float Mycila::JSY::Metrics::nominalPower() const { return activePower == 0 ? NAN : std::abs(voltage * voltage * current * current / activePower); }

void Mycila::JSY::Metrics::clear() {
  frequency = NAN;
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
  phaseAngleU = NAN;
  phaseAngleI = NAN;
  phaseAngleUI = NAN;
  thdU = NAN;
  thdI = NAN;
}

bool Mycila::JSY::Metrics::operator==(const Mycila::JSY::Metrics& other) const {
  return (std::isnan(frequency) ? std::isnan(other.frequency) : frequency == other.frequency) &&
         (std::isnan(voltage) ? std::isnan(other.voltage) : voltage == other.voltage) &&
         (std::isnan(current) ? std::isnan(other.current) : current == other.current) &&
         (std::isnan(activePower) ? std::isnan(other.activePower) : activePower == other.activePower) &&
         (std::isnan(reactivePower) ? std::isnan(other.reactivePower) : reactivePower == other.reactivePower) &&
         (std::isnan(apparentPower) ? std::isnan(other.apparentPower) : apparentPower == other.apparentPower) &&
         (std::isnan(powerFactor) ? std::isnan(other.powerFactor) : powerFactor == other.powerFactor) &&
         (std::isnan(activeEnergy) ? std::isnan(other.activeEnergy) : activeEnergy == other.activeEnergy) &&
         (std::isnan(activeEnergyImported) ? std::isnan(other.activeEnergyImported) : activeEnergyImported == other.activeEnergyImported) &&
         (std::isnan(activeEnergyReturned) ? std::isnan(other.activeEnergyReturned) : activeEnergyReturned == other.activeEnergyReturned) &&
         (std::isnan(reactiveEnergy) ? std::isnan(other.reactiveEnergy) : reactiveEnergy == other.reactiveEnergy) &&
         (std::isnan(reactiveEnergyImported) ? std::isnan(other.reactiveEnergyImported) : reactiveEnergyImported == other.reactiveEnergyImported) &&
         (std::isnan(reactiveEnergyReturned) ? std::isnan(other.reactiveEnergyReturned) : reactiveEnergyReturned == other.reactiveEnergyReturned) &&
         (std::isnan(apparentEnergy) ? std::isnan(other.apparentEnergy) : apparentEnergy == other.apparentEnergy) &&
         (std::isnan(phaseAngleU) ? std::isnan(other.phaseAngleU) : phaseAngleU == other.phaseAngleU) &&
         (std::isnan(phaseAngleI) ? std::isnan(other.phaseAngleI) : phaseAngleI == other.phaseAngleI) &&
         (std::isnan(phaseAngleUI) ? std::isnan(other.phaseAngleUI) : phaseAngleUI == other.phaseAngleUI) &&
         (std::isnan(thdU) ? std::isnan(other.thdU) : thdU == other.thdU) &&
         (std::isnan(thdI) ? std::isnan(other.thdI) : thdI == other.thdI);
}

Mycila::JSY::Metrics& Mycila::JSY::Metrics::operator+=(const Mycila::JSY::Metrics& other) {
  // voltage, frequency, phase angle and thd are not aggregated
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
  reactivePower = std::sqrt(apparentPower * apparentPower - activePower * activePower);
  return *this;
}

void Mycila::JSY::Metrics::operator=(const Metrics& other) {
  frequency = other.frequency;
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
  phaseAngleU = other.phaseAngleU;
  phaseAngleI = other.phaseAngleI;
  phaseAngleUI = other.phaseAngleUI;
  thdU = other.thdU;
  thdI = other.thdI;
}

#ifdef MYCILA_JSON_SUPPORT
void Mycila::JSY::Metrics::toJson(const JsonObject& root) const {
  if (!std::isnan(frequency))
    root["frequency"] = frequency;
  if (!std::isnan(voltage))
    root["voltage"] = voltage;
  if (!std::isnan(current))
    root["current"] = current;
  if (!std::isnan(activePower))
    root["active_power"] = activePower;
  if (!std::isnan(reactivePower))
    root["reactive_power"] = reactivePower;
  if (!std::isnan(apparentPower))
    root["apparent_power"] = apparentPower;
  if (!std::isnan(powerFactor))
    root["power_factor"] = powerFactor;
  if (!std::isnan(activeEnergy))
    root["active_energy"] = activeEnergy;
  if (!std::isnan(apparentEnergy))
    root["apparent_energy"] = apparentEnergy;
  if (!std::isnan(activeEnergyImported))
    root["active_energy_imported"] = activeEnergyImported;
  if (!std::isnan(activeEnergyReturned))
    root["active_energy_returned"] = activeEnergyReturned;
  if (!std::isnan(reactiveEnergy))
    root["reactive_energy"] = reactiveEnergy;
  if (!std::isnan(reactiveEnergyImported))
    root["reactive_energy_imported"] = reactiveEnergyImported;
  if (!std::isnan(reactiveEnergyReturned))
    root["reactive_energy_returned"] = reactiveEnergyReturned;
  if (!std::isnan(phaseAngleU))
    root["phase_angle_u"] = phaseAngleU;
  if (!std::isnan(phaseAngleI))
    root["phase_angle_i"] = phaseAngleI;
  if (!std::isnan(phaseAngleUI))
    root["phase_angle_ui"] = phaseAngleUI;
  if (!std::isnan(thdU))
    root["thd_u"] = thdU;
  if (!std::isnan(thdI))
    root["thd_i"] = thdI;
  float r = resistance();
  float d = dimmedVoltage();
  float n = nominalPower();
  float t = thdi();
  if (!std::isnan(r))
    root["resistance"] = r;
  if (!std::isnan(d))
    root["dimmed_voltage"] = d;
  if (!std::isnan(n))
    root["nominal_power"] = n;
  if (!std::isnan(t))
    root["thdi_0"] = t;
}
#endif
