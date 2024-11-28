#include <dfm-extension/dfm-extension.h>

#include "shredmenuplugin.h"

// 右键菜单的扩展
static DFMEXT::DFMExtMenuPlugin *shredMenu { nullptr };

extern "C" void dfm_extension_initiliaze()
{
    shredMenu = new dfm_extenison_shred::ShredMenuPlugin;
}

extern "C" void dfm_extension_shutdown()
{
    delete shredMenu;
}

extern "C" DFMEXT::DFMExtMenuPlugin *dfm_extension_menu()
{
    return shredMenu;
}
