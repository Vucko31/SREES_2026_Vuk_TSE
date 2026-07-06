#pragma once

#include <compiler/Definitions.h>
#include <sc/IPlugin.h>
#include <td/String.h>

#ifdef MU_WINDOWS
    #ifdef PLUGIN_EXPORTS
        #define PLUGIN_API __declspec(dllexport)
    #else
        #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #ifdef PLUGIN_EXPORTS
        #define PLUGIN_API __attribute__((visibility("default")))
    #else
        #define PLUGIN_API
    #endif
#endif

enum class InputKind
{
    Auto = 0,
    Matpower,
    SparseMTX
};

struct ConversionOptions
{
    InputKind inputKind = InputKind::Auto;
    bool writeComments = true;
};

void onClosedPluginWindow();
