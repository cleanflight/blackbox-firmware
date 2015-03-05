#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "serial.h"

static int rate_to_constant(int baudrate) {
    switch (baudrate) {
        case 9600:
            return B9600;
        case 19200:
            return B19200;
        case 38400:
            return B38400;
        case 57600:
            return B57600;
        case 115200:
            return B115200;
// Not all operating systems have constants for these faster sizes:
#ifdef B230400
        case 230400:
            return B230400;
#endif
#ifdef B460800
        case 460800:
            return B460800;
#endif
#ifdef B500000
        case 500000:
            return B500000;
#endif
#ifdef B576000
        case 576000;
            return B576000;
#endif
#ifdef B921600
        case 921600:
            return B921600;
#endif
#ifdef B1000000
        case 1000000:
            return B1000000;
#endif
#ifdef B1152000
        case 1152000:
            return B1152000;
#endif
        default:
            return 0;
    }
}

// From https://jim.sh/ftx/files/linux-custom-baudrate.c:
/* Open serial port in raw mode, with custom baudrate if necessary */
int serial_open(const char *device, int rate)
{
    struct termios options;
    int fd;
    int speed = 0;

    if ((fd = open(device, O_RDWR | O_NOCTTY)) == -1)
        return -1;

    speed = rate_to_constant(rate);

    if (speed == 0) {
        return -1;
        /* Custom divisor */
        /* struct serial_struct serinfo;
         *
         *
         * serinfo.reserved_char[0] = 0;
        if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0)
            return -1;
        serinfo.flags &= ~ASYNC_SPD_MASK;
        serinfo.flags |= ASYNC_SPD_CUST;
        serinfo.custom_divisor = (serinfo.baud_base + (rate / 2)) / rate;
        if (serinfo.custom_divisor < 1)
            serinfo.custom_divisor = 1;
        if (ioctl(fd, TIOCSSERIAL, &serinfo) < 0)
            return -1;
        if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0)
            return -1;
        if (serinfo.custom_divisor * rate != serinfo.baud_base) {
            warnx("actual baudrate is %d / %d = %f",
                  serinfo.baud_base, serinfo.custom_divisor,
                  (float)serinfo.baud_base / serinfo.custom_divisor);
        }*/
    }

    fcntl(fd, F_SETFL, 0);
    tcgetattr(fd, &options);
    cfsetispeed(&options, speed ?: B38400);
    cfsetospeed(&options, speed ?: B38400);
    cfmakeraw(&options);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~(PARENB | CRTSCTS | CSIZE | CSTOPB); // No hardware flow control, no parity, 1 stop bit, clear size
    options.c_cflag |= CS8; //8 bits

    if (tcsetattr(fd, TCSANOW, &options) != 0)
        return -1;

    return fd;
}
