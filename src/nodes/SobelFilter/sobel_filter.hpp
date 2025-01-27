//
// Plugin SobelFilter
//

#ifndef FLOWCV_PLUGIN_SOBEL_FILTER_HPP_
#define FLOWCV_PLUGIN_SOBEL_FILTER_HPP_
#include <DSPatch.h>

#include <nlohmann/json.hpp>

#include "Types.hpp"
#include "imgui_opencv.hpp"
#include "imgui_wrapper.hpp"

namespace DSPatch::DSPatchables {

class SobelFilter final : public Component
{
  public:
    SobelFilter();
    void UpdateGui(void *context, int interface) override;
    bool HasGui(int interface) override;
    std::string GetState() override;
    void SetState(std::string &&json_serialized) override;

  protected:
    void Process_(SignalBus const &inputs, SignalBus &outputs) override;

  private:
    int out_depth_;
    int derivative_order_;
    int ksize_;
    float scale_;
    float delta_;
};

}  // namespace DSPatch::DSPatchables

#endif  // FLOWCV_PLUGIN_SOBEL_FILTER_HPP_
