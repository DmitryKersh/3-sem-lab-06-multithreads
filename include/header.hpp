// Copyright 2020 Your Name <your_email>

#ifndef INCLUDE_HEADER_HPP_
#define INCLUDE_HEADER_HPP_

#include "../third-party/PicoSHA2/picosha2.h"
#include "boost/log/trivial.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <thread>
#include <sstream>
#include <mutex>
#include <csignal>
#include <optional>

using nlohmann::json;
using std::vector;
using std::ostringstream;
using std::mutex;
using std::optional;

#endif // INCLUDE_HEADER_HPP_
