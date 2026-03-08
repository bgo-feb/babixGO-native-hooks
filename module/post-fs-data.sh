#!/system/bin/sh

PACKAGE="com.scopely.monopolygo"
PAYLOAD="/system/lib64/libbabix_payload.so"
PROP="wrap.$PACKAGE"
VALUE="LD_PRELOAD=$PAYLOAD"

if [ ! -f "$PAYLOAD" ]; then
  log -t BabixHooks "payload missing: $PAYLOAD"
  exit 0
fi

if command -v resetprop >/dev/null 2>&1; then
  resetprop -n "$PROP" "$VALUE"
else
  setprop "$PROP" "$VALUE"
fi

log -t BabixHooks "configured $PROP=$VALUE"

