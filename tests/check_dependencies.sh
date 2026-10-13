#!/bin/sh
# Week 7 package rules, checked on the sources (run by ctest as 'dependencies'):
#   hal                    depends on nothing (only hal/ and the C++ standard library)
#   bme280 (core)          depends on hal (+ the vendored Bosch driver), not on station/platform
#   platform (arduino, linux)  depends on hal only, never on a sensor package
#   station (app library)  depends on hal only, never on a sensor package
# Only station_main (the composition root) and the examples name Bme280.
set -u
ROOT=$1
fail=0
check() {   # $1 = description, $2.. = command whose output must be empty
    desc=$1; shift
    out=$("$@")
    if [ -n "$out" ]; then echo "FAIL: $desc"; echo "$out"; fail=1; else echo "ok: $desc"; fi
}
cd "$ROOT" || exit 1
check "hal includes only hal/ and std headers" \
    sh -c 'grep -nE "#include" include/hal/*.hpp | grep -vE "#include \"(Bus|Clock|EnvironmentSensor|Types)\.hpp\"|#include <c?[a-z_]+>"'
check "bme280 core does not include station/ or platform/" \
    sh -c 'grep -nE "#include .*(station|platform)/" include/bme280/*.hpp src/Bme280.cpp'
check "platform does not include a sensor package" \
    sh -c 'grep -rnE "#include .*bme280/" platform'
check "station library does not include a sensor package" \
    sh -c 'grep -rnE "#include .*bme280/|bme280::" include/station station'
exit $fail
