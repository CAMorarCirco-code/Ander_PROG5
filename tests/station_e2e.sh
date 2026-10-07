#!/bin/sh
# End-to-end test of the composition root (station_main) on the host.
#   station_e2e.sh <station_main> <mosquitto> <mosquitto_sub>
# (mosquitto_pub is taken from the same directory as mosquitto_sub)
# Exit 0 = pass, 1 = fail, 77 = skipped (CTest SKIP_RETURN_CODE).
# station_main always talks to localhost:1883 (fixed in main), so the
# broker part needs that port to be free.
set -u
MAIN=$1; BROKER=$2; SUB=$3
TOPIC='han/ese/ander/bme280/state'
PUB=$(dirname "$SUB")/mosquitto_pub
BPID=
cleanup() { if [ -n "$BPID" ]; then kill "$BPID" 2>/dev/null; wait "$BPID" 2>/dev/null; fi; rm -rf "$WORK"; }
WORK=$(mktemp -d); trap cleanup EXIT
# mosquitto_pub succeeds as soon as it can connect: a cheap "is a broker up?"
brokerUp() { "$PUB" -h localhost -p 1883 -t e2e/probe -m x >/dev/null 2>&1; }
fail() { echo "FAIL: $*"; exit 1; }

# 1. --fake --console: 4 samples in 3.5 s, exact payloads, clean SIGINT exit
timeout --preserve-status -s INT 3.5 "$MAIN" --fake --console > "$WORK/console" 2>/dev/null || fail "console run exit $?"
head -n 4 "$WORK/console" > "$WORK/console4"
printf '%s {"t":20.00,"p":101325,"h":45.2}\n%s {"t":20.10,"p":101325,"h":45.2}\n%s {"t":20.20,"p":101325,"h":45.2}\n%s {"t":20.30,"p":101325,"h":45.2}\n' \
    "$TOPIC" "$TOPIC" "$TOPIC" "$TOPIC" | cmp -s - "$WORK/console4" || { cat "$WORK/console"; fail "console output"; }
echo "ok: --fake --console prints the ramp on $TOPIC"

# 2. real hardware path without /dev/i2c-1 (or without a sensor): clean error, exit 1
if [ ! -e /dev/i2c-1 ]; then
    "$MAIN" > /dev/null 2> "$WORK/hw" && fail "no-hardware run should fail"
    grep -q "/dev/i2c-1" "$WORK/hw" || fail "no-hardware message: $(cat "$WORK/hw")"
    echo "ok: no /dev/i2c-1 -> clean error exit"
fi

if [ ! -x "$BROKER" ] || [ ! -x "$SUB" ] || [ ! -x "$PUB" ]; then echo "SKIP: no mosquitto broker/clients"; exit 77; fi
if brokerUp; then
    echo "SKIP: something already listens on localhost:1883"; exit 77
fi

# 3. broker down: every publish fails, the sampler keeps going, clean exit
timeout --preserve-status -s INT 3.5 "$MAIN" --fake > /dev/null 2> "$WORK/down" || fail "broker-down run exit $?"
[ "$(grep -c 'publish failed' "$WORK/down")" -ge 3 ] || fail "broker down: $(cat "$WORK/down")"
echo "ok: broker down -> publish() false every second, station keeps sampling"

# 4. real broker: mosquitto_sub -t 'han/ese/#' -v sees the payloads
"$BROKER" -p 1883 > /dev/null 2>&1 & BPID=$!
i=0; until brokerUp; do i=$((i+1)); [ $i -ge 50 ] && fail "broker did not start"; sleep 0.1; done
"$SUB" -h localhost -t 'han/ese/#' -v -C 3 -W 15 > "$WORK/capture" 2>&1 & SPID=$!
sleep 0.5
timeout --preserve-status -s INT 5.5 "$MAIN" --fake 2>/dev/null || fail "broker run exit $?"
wait $SPID || fail "mosquitto_sub got fewer than 3 messages: $(cat "$WORK/capture")"
grep -Eqv "^$TOPIC \{\"t\":2[0-4]\.[0-9]0,\"p\":101325,\"h\":45\.2\}$" "$WORK/capture" && fail "unexpected line: $(cat "$WORK/capture")"
cat "$WORK/capture"
echo "ok: mosquitto_sub -t 'han/ese/#' -v received 3 messages on $TOPIC"
