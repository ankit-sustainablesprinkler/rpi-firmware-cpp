#include  "serial_driver.h"

#include  <boost/asio.hpp>
#include  <boost/signals2.hpp>
#include  <boost/system/error_code.hpp>
#include  <boost/date_time/posix_time/posix_time.hpp>
#include  <boost/date_time/posix_time/posix_time_duration.hpp>


using namespace std;
using namespace boost;
using namespace boost::asio;


enum ReadResult
{
    resultInProgress,
    resultSuccess,
    resultError,
    resultTimeout
};


class SerialDriverImpl : private boost::noncopyable
{
private:
    io_service io; ///< Io service object
    serial_port port; ///< Serial port object
    deadline_timer timer; ///< Timer for timeout
    posix_time::time_duration timeout; ///< Read/write timeout
    enum ReadResult result;  ///< Used by read with timeout
    streamsize totalTransferred; ///< Used by async read callback
    char *readBuffer; ///< Used to hold read data
    streamsize readBufferSize; ///< Size of read data buffer

public:
    SerialDriverImpl();

    bool open(boost::string_ref device, int baudrate);
    void close();

    bool is_open(void) { return port.is_open(); };

    streamsize read(char *s, streamsize n);

    streamsize write(boost::string_ref s);
    void timeoutExpired(const boost::system::error_code &error);
    void readCompleted(const boost::system::error_code &error,
            const streamsize bytesTransferred);
};

SerialDriverImpl::SerialDriverImpl()
        : io(), port(io), timer(io), timeout(posix_time::milliseconds(100)),
        result(resultError), totalTransferred(0), readBuffer(),
        readBufferSize(0)
{
    if (timeout == posix_time::seconds(0))
    	timeout = posix_time::max_date_time;
}

bool SerialDriverImpl::open(boost::string_ref device, int baudrate)
{
    try {
    	close();

    	port.open(device.to_string());

    	port.set_option(serial_port_base::baud_rate(baudrate));
    	port.set_option(serial_port_base::parity(serial_port_base::parity::none));
    	port.set_option(serial_port_base::character_size(8));
    	port.set_option(serial_port_base::flow_control(serial_port_base::flow_control::hardware));
    	port.set_option(serial_port_base::stop_bits(serial_port_base::stop_bits::one));

    } catch(const std::exception &e) {
        throw ios::failure(e.what());
    }

    return port.is_open();
}

void SerialDriverImpl::close(void)
{
	if (port.is_open())
		port.close();
}

streamsize SerialDriverImpl::read(char *s, streamsize n)
{
    result=resultInProgress;
    totalTransferred = 0;
    readBuffer = s;
    readBufferSize = n;

    timer.expires_from_now(timeout);
    timer.async_wait(boost::bind(&SerialDriverImpl::timeoutExpired, this,
            boost::asio::placeholders::error));

    port.async_read_some(buffer(s,n),
    		boost::bind(&SerialDriverImpl::readCompleted, this,
    		boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));

    for(;;)
    {
        io.run_one();
        switch(result)
        {
            case resultSuccess:
                timer.cancel();
                readBuffer[totalTransferred] = '\0';
                return totalTransferred;

            case resultTimeout:
                port.cancel();
                if (totalTransferred)
                {
                	result = resultSuccess;
                    readBuffer[totalTransferred] = '\0';
                    return totalTransferred;
                }
                throw(TimeoutException("Timeout expired"));

            case resultError:
                port.cancel();
                timer.cancel();
                throw(ios_base::failure("Error while reading"));

            case resultInProgress:
            default:
                break;
        }
    }

    return 0;
}

streamsize SerialDriverImpl::write(boost::string_ref s)
{
    try {
        return asio::write(port,asio::buffer(s.to_string()));
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        throw(ios_base::failure(e.what()));
    }
}

void SerialDriverImpl::timeoutExpired(const boost::system::error_code &error)
{
	if(!error && (result == resultInProgress))
		result = resultTimeout;
}

void SerialDriverImpl::readCompleted(const boost::system::error_code &error,
        const streamsize transferred)
{
    if(!error)
    {
        totalTransferred += transferred;

    	if (totalTransferred != readBufferSize) {
            port.async_read_some(
                    asio::buffer(&readBuffer[totalTransferred], readBufferSize - totalTransferred),
                    boost::bind(&SerialDriverImpl::readCompleted, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
            return;
    	}

        result = resultSuccess;
        return;
    }

    //In case a asynchronous operation is cancelled due to a timeout,
    //each OS seems to have its way to react.
    #ifdef _WIN32
    if(error.value()==995) return; //Windows spits out error 995
    #elif defined(__APPLE__)
    if(error.value()==45)
    {
        //Bug on OS X, it might be necessary to repeat the setup
        //http://osdir.com/ml/lib.boost.asio.user/2008-08/msg00004.html
        port.async_read_some(
                asio::buffer(readBuffer,readBufferSize),
                boost::bind(&SerialDriverImpl::readCompleted, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        return;
    }
    #else //Linux
    if(error.value()==125) return; //Linux outputs error 125
    #endif

    result = resultError;
}


SerialDriver::SerialDriver() :
	serial(new SerialDriverImpl()), baudrate(0)
{
}

SerialDriver::~SerialDriver()
{
	delete serial;
	serial = nullptr;
}

bool SerialDriver::open(boost::string_ref device, int baudrate)
{
	this->device = device;
	this->baudrate = baudrate;

	return serial->open(device, baudrate);
}

void SerialDriver::close(void)
{
	serial->close();
}

bool SerialDriver::is_open(void)
{
	return serial->is_open();
}

void SerialDriver::restart()
{
	serial->close();
	serial->open(device, baudrate);
}

streamsize  SerialDriver::read(char * s, streamsize len)
{
	return serial->read(s, len);
}

streamsize SerialDriver::write(boost::string_ref s)
{
	return serial->write(s);
}
