#ifndef  __FAILURE_EXCEPTION_H__
#define  __FAILURE_EXCEPTION_H__


#include  <boost/lexical_cast.hpp>

class FailureException: public std::exception
{
private:
	std::string title_;
	std::string status_;
	std::string hdr;
	std::string msg;

public:
    FailureException(const std::string& title,
    		const std::string& msg,
			const std::string& status)
    : title_(title), status_(status), hdr(status + " " + title), msg(msg)
  {}

  virtual const char* what() const throw()
  {
    return msg.c_str();
  }

  const char* title() const throw()
  {
    return title_.c_str();
  }

  const char* header() const throw()
  {
    return hdr.c_str();
  }

  const char* status() const throw()
  {
    return status_.c_str();
  }
};

#endif
