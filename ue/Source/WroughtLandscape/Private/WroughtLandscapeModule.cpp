#include "Modules/ModuleManager.h"

// A plain editor module with no custom startup — it exists only to carry
// UWroughtLandscapeLibrary. IMPLEMENT_MODULE is mandatory or the module won't load.
IMPLEMENT_MODULE(FDefaultModuleImpl, WroughtLandscape);
