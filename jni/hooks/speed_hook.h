#pragma once

namespace Hooks::Speed {

bool Install();

// Speed multiplier control
void SetSpeedMultiplier(float multiplier);
float GetSpeedMultiplier();
void EnableSpeedModification(bool enabled);
bool IsSpeedModificationEnabled();

}
