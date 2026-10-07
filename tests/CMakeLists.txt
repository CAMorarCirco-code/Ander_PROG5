# Core tests: only bme280_core, a MockBus and a FakeClock.
add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE bme280_core)
bme280_set_warnings(test_core)
add_test(NAME core COMMAND test_core)

