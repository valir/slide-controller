
#pragma once

#include "bsec2.h"
#include "bsec_datatypes.h"
#include "bme-sensor.h"

class bme680Sensor : public bmeSensor<BME68X_INTF_RET_TYPE, BME68X_OK, BME68X_E_COM_FAIL> {
  public:
  void init();
  void sensors_timer();

  private:
  static void bme_delay_us(uint32_t period, void*);
  static void sensors_state_backup(void*);
  static void restore_sensors_state();
  static void sensors_data_callback(
      const bme68xData data, const bsecOutputs outputs, const Bsec2 bsec);
};
