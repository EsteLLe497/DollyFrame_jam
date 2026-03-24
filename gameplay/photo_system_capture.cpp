#include "photo_system.h"
#include "photo_system_bridge.h"

namespace photo_system
{
void HandleCapture(GameScene& scene)
{
    photo_system_bridge::HandleCaptureBridge(scene);
}
}
