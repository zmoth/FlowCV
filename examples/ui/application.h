#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include <DSPatch.h>

#include <deque>
#include <nlohmann/json.hpp>

#include "../managers/FlowCV_Manager.hpp"
#include "Types.hpp"
#include "imgui_internal.h"
#include "imgui_wrapper.hpp"
#include "node_editor.h"
#include "utils/settings.h"

struct ApplicationGlobals
{
    uint64_t selectedId{};
    bool showLoadDialog{};
    bool showSaveDialog{};
    bool newFlow{};
    bool saveFlow{};
    bool firstLoad{};
    bool stateHasChanged{};
    bool stateIndicatorOnce{};
    bool allowEditorKeys{};
    bool doMenuCut{};
    bool doMenuCopy{};
    bool doMenuPaste{};
};

ApplicationGlobals *GetApplicationGlobals();
const char *Application_GetName();
void Application_Initialize(std::string &appPath);
void Application_Finalize();
void Application_Frame(FlowCV::FlowCV_Manager &flowMan, const AppSettings &settings);
nlohmann::json Application_GetState(FlowCV::FlowCV_Manager &flowMan);
bool Application_SetState(FlowCV::FlowCV_Manager &flowMan, nlohmann::json &state);