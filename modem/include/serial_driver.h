#ifndef __SERIAL_DRIVER_H__
#define __SERIAL_DRIVER_H__

#include  <boost/utility/string_ref.hpp>


class SerialDriverImpl;


class SerialDriver
{
public:
    SerialDriver();
	~SerialDriver();
    std::streamsize write(boost::string_ref s);
    std::streamsize read(char *s, std::streamsize n);

    void restart(void);

    bool open(boost::string_ref device, int baudrate);
    void close();
    bool is_open();

private:
    SerialDriverImpl *serial;
    boost::string_ref device;
    int baudrate;
};


class TimeoutException: public std::ios_base::failure
{
public:
    TimeoutException(const std::string& arg): failure(arg) {}
};

#endif
