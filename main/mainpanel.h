
#pragma once

#include "events.h"

class MainPanel : public EventObserver {
  public:
  MainPanel(lv_obj_t* parent);
  void update();

  void notice(const Event&) override ;
  const char* name() override { return "MainPanel"; }

  void setTemp(float temp);
  void setHumidity(float humidity);
  void setCO2(float co2);
  void setIAQ(float iaq);
};
