#include "Modules/ModuleManager.h"

// WroughtSim is the project's single, primary game module. Without a module
// implementation the dylib loads but exports no IModuleInterface, so the editor
// reports "could not be successfully initialized after it was loaded". This is
// the standard default impl -- no custom StartupModule/ShutdownModule needed.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, WroughtSim, "WroughtSim");
