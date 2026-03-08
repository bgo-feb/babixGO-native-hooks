#pragma once

namespace Hooks::Roll {

bool Install();

// Trigger a roll with specified multiplier
// multiplier: The multiplier to use (1, 5, 10, 50, 100)
// Returns true if roll was triggered successfully
bool TriggerRoll(int multiplier);

// Get/Set the stored RollService instance
void SetRollServiceInstance(void* instance);
void* GetRollServiceInstance();

}
