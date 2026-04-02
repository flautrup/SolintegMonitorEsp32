#pragma once
#include <stdint.h>

// Current live data from the Solinteg system
typedef struct {
    float solar_power_kw;       // Current solar PV production (kW)
    float grid_power_kw;        // Grid power: positive = import, negative = export
    float battery_power_kw;     // Battery power: positive = charging, negative = discharging
    float home_consumption_kw;  // Current home load (kW)
    float battery_soc_pct;      // Battery state of charge (%)
    bool  data_valid;           // True if the last API call succeeded
} SolintegCurrentData;

// Daily statistics
typedef struct {
    float max_solar_kw;         // Peak solar production for the day (kW)
    float max_consumption_kw;   // Peak home consumption for the day (kW)
    float total_solar_kwh;      // Total solar energy generated today (kWh)
    float total_consumption_kwh;// Total home energy consumed today (kWh)
    bool  data_valid;
} SolintegDailyData;
