#ifndef LOGGING_HXX
#define LOGGING_HXX

#include <iostream>
#include <string>

class Logger {
public:
  enum LogLevel {
    LL_spam,
    LL_debug,
    LL_info,
    LL_warning,
    LL_error,
    LL_fatal,
  };

  template<class T>
  inline Logger &info(const T &data);

private:
  std::string _category_name;
  LogLevel _log_level;
};

template<class T>
inline Logger &Logger::
info(const T &data) {
  if (_log_level <= LL_info) {

  }
}

#endif // LOGGING_HXX
