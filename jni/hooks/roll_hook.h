#pragma once

namespace Hooks::Roll {

bool Install();

// Manual roll trigger - calls AttemptRoll on the RollService instance
// Returns true if successful, false if hook not installed or no instance available
bool TriggerAttemptRoll();

}
