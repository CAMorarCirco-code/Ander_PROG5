#include "LinuxI2cBus.hpp"

#include <array>
#include <cerrno>
#include <system_error>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace bme280 {

LinuxI2cBus::LinuxI2cBus(uint8_t address, const std::string& device)
    : device_(device), address_(address)
{
    fd_ = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        throw std::system_error(errno, std::generic_category(), "open " + device_);
    }
    // I2C_SLAVE makes the kernel check the address and refuse it (EBUSY) if a
    // kernel driver is bound to it. The transfers below carry the address in
    // each message, so this is the only place it is "selected".
    if (::ioctl(fd_, I2C_SLAVE, static_cast<unsigned long>(address_)) < 0) {
        const int err = errno;
        ::close(fd_);   // the destructor does not run for a throwing constructor
        fd_ = -1;
        throw std::system_error(err, std::generic_category(), "ioctl I2C_SLAVE on " + device_);
    }
}

LinuxI2cBus::~LinuxI2cBus()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool LinuxI2cBus::read(uint8_t reg, uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0 || len > kMaxTransfer) {
        return false;
    }
    // A plain write() + read() would put a STOP between register pointer and
    // data. I2C_RDWR sends both messages as one combined transaction:
    //   START addr+W reg  REPEATED-START addr+R data... STOP
    std::array<i2c_msg, 2> msgs{};
    msgs[0].addr  = address_;
    msgs[0].flags = 0;
    msgs[0].len   = 1;
    msgs[0].buf   = &reg;
    msgs[1].addr  = address_;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = static_cast<__u16>(len);
    msgs[1].buf   = data;

    i2c_rdwr_ioctl_data xfer{};
    xfer.msgs  = msgs.data();
    xfer.nmsgs = static_cast<__u32>(msgs.size());
    // Returns the number of messages transferred; anything else is a failure
    // (NACK -> EREMOTEIO, timeout, bus error).
    return ::ioctl(fd_, I2C_RDWR, &xfer) == static_cast<int>(msgs.size());
}

bool LinuxI2cBus::write(uint8_t reg, const uint8_t* data, size_t len)
{
    // The Bosch driver already interleaves further register/value pairs into
    // `data`, so one message of [reg, data...] is all it takes.
    if ((data == nullptr && len != 0) || len + 1 > kMaxTransfer) {
        return false;
    }
    std::array<uint8_t, kMaxTransfer> buf{};
    buf[0] = reg;
    for (size_t i = 0; i < len; ++i) {
        buf[i + 1] = data[i];
    }

    i2c_msg msg{};
    msg.addr  = address_;
    msg.flags = 0;
    msg.len   = static_cast<__u16>(len + 1);
    msg.buf   = buf.data();

    i2c_rdwr_ioctl_data xfer{};
    xfer.msgs  = &msg;
    xfer.nmsgs = 1;
    return ::ioctl(fd_, I2C_RDWR, &xfer) == 1;
}

} // namespace bme280
