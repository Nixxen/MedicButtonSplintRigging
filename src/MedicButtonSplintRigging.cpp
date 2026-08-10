#include <Debug.h>

#include <core/Functions.h>

#include <kenshi/GameWorld.h>
#include <kenshi/Globals.h>
#include <kenshi/gui/TitleScreen.h>

#include "version.h"

__declspec(dllexport) void startPlugin()
{
    DebugLog("v" MBSR_VERSION_STRING " loaded");
}