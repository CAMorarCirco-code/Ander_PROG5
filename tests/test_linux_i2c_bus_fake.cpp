// LinuxI2cBus against a fake kernel: this executable defines its own
// ioctl(), which the linker picks instead of the libc one for the calls made
// from LinuxI2cBus.cpp. The fake checks every I2C_RDWR transaction for the
// exact message layout (address, flags, lengths) and serves it from a
// FakeBme280, so the unchanged core runs end-to-end over LinuxI2cBus and
// LinuxClock without any I2C hardware.
//
// Deliberately does NOT include <sys/ioctl.h>, whose declaration of ioctl
// would clash with the definition below.

#include <bme280/Bme280.hpp>

#include "FakeBme280.hpp"
#include "FdCount.hpp"
#include "LinuxClock.hpp"
#include "LinuxI2cBus.hpp"
#include "TestMain.hpp"

#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include <cerrno>
#include <cstdarg>
#include <system_error>
#include <vector>

namespace {

struct Transfer {
    std::vector<i2c_msg>              msgs;
    std::vector<std::vector<uint8_t>> payload;   // copy of write buffers
};

struct FakeKernel {
    test::FakeBme280      chip;
    unsigned long         slaveAddr = 0;
    int                   slaveErrno = 0;    // != 0: I2C_SLAVE fails with it
    int                   rdwrErrno = 0;     // != 0: I2C_RDWR fails with it
    uint8_t               chipAddress = 0x76;
    std::vector<Transfer> log;
    int                   unexpected = 0;    // malformed transactions seen
};

FakeKernel g_kernel;

int fakeRdwr(const i2c_rdwr_ioctl_data* x)
{
    Transfer t;
    for (__u32 i = 0; i < x->nmsgs; ++i) {
        t.msgs.push_back(x->msgs[i]);
        t.payload.emplace_back(x->msgs[i].buf, x->msgs[i].buf + x->msgs[i].len);
    }
    g_kernel.log.push_back(t);

    if (g_kernel.rdwrErrno != 0) {
        errno = g_kernel.rdwrErrno;
        return -1;
    }
    for (__u32 i = 0; i < x->nmsgs; ++i) {
        if (x->msgs[i].addr != g_kernel.chipAddress) {
            errno = EREMOTEIO;   // nobody ACKs this address
            return -1;
        }
    }
    // Register read: [W reg] + repeated start + [R n].
    if (x->nmsgs == 2 && x->msgs[0].flags == 0 && x->msgs[0].len == 1 && x->msgs[1].flags == I2C_M_RD &&
        x->msgs[1].len > 0) {
        g_kernel.chip.read(x->msgs[0].buf[0], x->msgs[1].buf, x->msgs[1].len);
        return 2;
    }
    // Register write: [W reg data...].
    if (x->nmsgs == 1 && x->msgs[0].flags == 0 && x->msgs[0].len >= 2) {
        g_kernel.chip.write(x->msgs[0].buf[0], x->msgs[0].buf + 1, x->msgs[0].len - 1U);
        return 1;
    }
    ++g_kernel.unexpected;
    errno = EINVAL;
    return -1;
}

} // namespace

namespace {

int fakeIoctl(unsigned long request, void* arg)
{
    if (request == I2C_SLAVE) {
        if (g_kernel.slaveErrno != 0) {
            errno = g_kernel.slaveErrno;
            return -1;
        }
        g_kernel.slaveAddr = reinterpret_cast<unsigned long>(arg);
        return 0;
    }
    if (request == I2C_RDWR) {
        return fakeRdwr(static_cast<const i2c_rdwr_ioctl_data*>(arg));
    }
    errno = ENOTTY;
    return -1;
}

void* firstVarArg(va_list ap)
{
    return va_arg(ap, void*);
}

} // namespace

extern "C" int ioctl(int fd, unsigned long request, ...)
{
    (void)fd;
    va_list ap;
    va_start(ap, request);
    void* arg = firstVarArg(ap);
    va_end(ap);
    return fakeIoctl(request, arg);
}

#if defined(__USE_TIME_BITS64) && __TIMESIZE == 32
// 32-bit targets built with 64-bit time_t (e.g. Debian/Ubuntu armhf, Raspberry
// Pi OS 32-bit) redirect ioctl() to this glibc symbol; fake it as well.
extern "C" int __ioctl_time64(int fd, unsigned long request, ...)
{
    (void)fd;
    va_list ap;
    va_start(ap, request);
    void* arg = firstVarArg(ap);
    va_end(ap);
    return fakeIoctl(request, arg);
}
#endif

namespace {

// /dev/null stands in for /dev/i2c-1: open() and close() are real, only
// ioctl() is faked.
constexpr const char* kDevice = "/dev/null";

void reset()
{
    g_kernel = FakeKernel{};
}

void constructorSelectsSlaveAddress()
{
    reset();
    bme280::LinuxI2cBus bus(0x77, kDevice);
    CHECK(g_kernel.slaveAddr == 0x77);
    CHECK(bus.address() == 0x77);
    CHECK(bus.device() == kDevice);
}

void readIsOneCombinedTransactionWithRepeatedStart()
{
    reset();
    bme280::LinuxI2cBus bus(0x76, kDevice);
    uint8_t id = 0;
    CHECK(bus.read(0xD0, &id, 1));
    CHECK(id == 0x60);
    CHECK(g_kernel.log.size() == 1);
    if (g_kernel.log.size() == 1) {
        const Transfer& t = g_kernel.log[0];
        CHECK(t.msgs.size() == 2);
        // msg 0: write the register pointer, no STOP afterwards (same ioctl)
        CHECK(t.msgs[0].addr == 0x76 && t.msgs[0].flags == 0 && t.msgs[0].len == 1);
        CHECK(t.payload[0].size() == 1 && t.payload[0][0] == 0xD0);
        // msg 1: read with repeated start
        CHECK(t.msgs[1].addr == 0x76 && t.msgs[1].flags == I2C_M_RD && t.msgs[1].len == 1);
    }

    uint8_t calib[26] = {};
    CHECK(bus.read(0x88, calib, sizeof calib));
    CHECK(g_kernel.log.size() == 2 && g_kernel.log[1].msgs[1].len == 26);
    CHECK(calib[0] == (27504 & 0xFF) && calib[1] == (27504 >> 8));
}

void writeIsOneMessageRegisterThenData()
{
    reset();
    bme280::LinuxI2cBus bus(0x76, kDevice);
    // Bosch burst format: value for reg, then (reg, value) pairs.
    const uint8_t data[] = {0x05, 0xF4, 0x25};   // 0x25: osrs_t x1, osrs_p x1, forced
    CHECK(bus.write(0xF2, data, sizeof data));
    CHECK(g_kernel.log.size() == 1);
    if (g_kernel.log.size() == 1) {
        const Transfer& t = g_kernel.log[0];
        CHECK(t.msgs.size() == 1);
        CHECK(t.msgs[0].addr == 0x76 && t.msgs[0].flags == 0 && t.msgs[0].len == 4);
        CHECK((t.payload[0] == std::vector<uint8_t>{0xF2, 0x05, 0xF4, 0x25}));
    }
    CHECK(g_kernel.chip.regs[0xF2] == 0x05);
    CHECK(g_kernel.chip.regs[0xF4] == 0x24);   // forced bit dropped by the fake chip
}

void invalidLengthsAreRejectedWithoutBusTraffic()
{
    reset();
    bme280::LinuxI2cBus bus(0x76, kDevice);
    uint8_t buf[64] = {};
    CHECK(!bus.read(0x88, buf, 0));
    CHECK(!bus.read(0x88, buf, 33));
    CHECK(!bus.read(0x88, nullptr, 1));
    CHECK(!bus.write(0xF4, buf, 32));   // 32 + register byte > 32
    CHECK(bus.write(0xF4, buf, 31));    // exactly 32 on the wire
    CHECK(bus.read(0x88, buf, 32));
    CHECK(g_kernel.log.size() == 2);
}

void kernelErrorsBecomeFalse()
{
    reset();
    bme280::LinuxI2cBus bus(0x76, kDevice);
    g_kernel.rdwrErrno = EREMOTEIO;   // NACK
    uint8_t b = 0;
    CHECK(!bus.read(0xD0, &b, 1));
    CHECK(!bus.write(0xF4, &b, 1));
}

void slaveSelectFailureThrowsAndDoesNotLeakFd()
{
    reset();
    g_kernel.slaveErrno = EBUSY;   // e.g. the bmp280 kernel driver owns 0x76
    const int fdsBefore = test::openFdCount();
    bool threw = false;
    try {
        bme280::LinuxI2cBus bus(0x76, kDevice);
    } catch (const std::system_error& e) {
        threw = e.code().value() == EBUSY;
    }
    CHECK(threw);
    CHECK(test::openFdCount() == fdsBefore);
}

void coreRunsEndToEndOverLinuxBus()
{
    reset();
    bme280::LinuxI2cBus bus(0x76, kDevice);
    bme280::LinuxClock  clock;   // real clock_nanosleep
    bme280::Bme280      sensor(bus, clock);

    CHECK(sensor.init() == bme280::Error::None);
    CHECK(g_kernel.chip.softResets == 1);

    bme280::Measurement m;
    CHECK(sensor.readForced(m) == bme280::Error::None);
    CHECK(g_kernel.chip.forcedTriggers == 1);
    CHECK_NEAR(m.temperatureC, 25.08, 0.005);
    CHECK_NEAR(m.pressurePa, 100653.0, 2.0);
    CHECK(m.humidityPct >= 0.0F && m.humidityPct <= 100.0F);

    // Every transaction the core produced had one of the two valid shapes.
    CHECK(g_kernel.unexpected == 0);
    for (const Transfer& t : g_kernel.log) {
        CHECK(t.msgs.size() == 1 || t.msgs.size() == 2);
        if (t.msgs.size() == 2) {
            CHECK(t.msgs[0].len == 1 && t.msgs[0].flags == 0 && t.msgs[1].flags == I2C_M_RD);
        }
    }
}

void wrongAddressGivesBusFailure()
{
    reset();
    g_kernel.chipAddress = 0x77;   // sensor strapped high, program uses 0x76
    bme280::LinuxI2cBus bus(0x76, kDevice);
    bme280::LinuxClock  clock;
    bme280::Bme280      sensor(bus, clock);
    CHECK(sensor.init() == bme280::Error::BusFailure);
}

} // namespace

int main()
{
    RUN(constructorSelectsSlaveAddress);
    RUN(readIsOneCombinedTransactionWithRepeatedStart);
    RUN(writeIsOneMessageRegisterThenData);
    RUN(invalidLengthsAreRejectedWithoutBusTraffic);
    RUN(kernelErrorsBecomeFalse);
    RUN(slaveSelectFailureThrowsAndDoesNotLeakFd);
    RUN(coreRunsEndToEndOverLinuxBus);
    RUN(wrongAddressGivesBusFailure);
    return TEST_RESULT();
}
