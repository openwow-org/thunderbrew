#ifndef GX_C_GX_VIDEO_OPTIONS_HPP
#define GX_C_GX_VIDEO_OPTIONS_HPP

#include "ui/Types.hpp"
#include <cstdint>

class CGVideoOptions {
    public:
    // Static variables
    static FrameScript_Method s_ScriptFunctions[];

    // Static functions
    static void RegisterScriptFunctions();
};

#endif