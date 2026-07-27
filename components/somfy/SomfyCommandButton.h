#pragma once
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "SomfyCover.h"

namespace esphome {
namespace somfy {

// A single one-tap HA button that dispatches one debug/programming command
// (see the cmd table in SomfyCover.h) to a specific cover. Exists so those
// commands don't have to be sent by dragging the cover's tilt slider to a
// magic number anymore -- see get_traits() in SomfyCover.h for why that
// mattered.
class SomfyCommandButton : public button::Button, public Component {
 public:
  void set_parent(SomfyCover *parent) { parent_ = parent; }
  void set_cmd(int cmd) { cmd_ = cmd; }

 protected:
  void press_action() override {
    if (parent_ != nullptr) {
      parent_->run_command(cmd_);
    }
  }

  SomfyCover *parent_{nullptr};
  int cmd_{0};
};

}  // namespace somfy
}  // namespace esphome
