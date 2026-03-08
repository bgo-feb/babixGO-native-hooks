#!/system/bin/sh

PACKAGE="com.scopely.monopolygo"
PROP="wrap.$PACKAGE"

# Zygisk loader is now responsible for loading libbabix_payload.so.
# Clear any stale wrap property from older module versions.
if command -v resetprop >/dev/null 2>&1; then
  resetprop -n "$PROP" ""
else
  setprop "$PROP" ""
fi

log -t BabixHooks "zygisk mode active; cleared $PROP"
