// Copyright 2020 Your Name <your_email>

#include <../third-party/PicoSHA2/picosha2.h>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/keywords/file_name.hpp>
#include <boost/log/keywords/rotation_size.hpp>
#include <boost/log/keywords/time_based_rotation.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <csignal>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>

using nlohmann::json;
using std::mutex;
using std::optional;
using std::ostringstream;
using std::vector;

mutex mtx;

static ::std::atomic_bool shutdown = false;

void setup_logging() {
  namespace logging = ::boost::log;
  namespace sources = logging::sources;
  namespace expressions = logging::expressions;
  namespace keywords = logging::keywords;
  namespace sinks = logging::sinks;

  logging::add_common_attributes();

  logging::core::get()->set_filter(logging::trivial::severity >=
                                   logging::trivial::info);

  logging::add_file_log(keywords::file_name = "logs/file_%3N.log",
                        keywords::rotation_size = 10 * 1024,
                        keywords::time_based_rotation =
                            sinks::file::rotation_at_time_point(12, 0, 0));
}

bool is_appropriate(const std::string& s, const std::string& suffix) {
  return s.substr(s.length() - suffix.length()) == suffix;
}

std::string random_hex_string() {
  auto prototype1 = rand();
  auto prototype2 = rand();
  std::string s(16, '0');
  for (size_t i = 0; i < 8; i++) {
    s[i] = ((prototype1 % 16) > 9) ? (prototype1 % 16 + '0' + 7)
                                   : (prototype1 % 16 + '0');
    prototype1 /= 16;
  }
  for (size_t i = 8; i < 16; i++) {
    s[i] = ((prototype2 % 16) > 9) ? (prototype2 % 16 + '0' + 7)
                                   : (prototype2 % 16 + '0');
    prototype2 /= 16;
  }
  return s;
}

optional<json> find_prototype(const std::string& suffix) {
  json j;
  std::string data_string = random_hex_string();

  while (!is_appropriate(picosha2::hash256_hex_string(data_string), suffix)) {
    data_string = random_hex_string();
    if (shutdown) {
      return std::nullopt;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "Found! Data: " << data_string << "; SHA256: "
                          << picosha2::hash256_hex_string(data_string);

  j["timestamp"] = std::to_string(clock()) + " (" +
                   std::to_string((clock() / 1000)) + " ms.)";
  j["hash"] = picosha2::hash256_hex_string(data_string);
  j["data"] = data_string;

  return {j};
}

void write_to_json(json& output) {
  optional<json> j = find_prototype("0000");
  while (!shutdown) {
    if (!j) {
      return;
    }

    mtx.lock();
    output.push_back(*j);
    mtx.unlock();
    j = find_prototype("0000");
  }
}

int main(int argc, char* args[]) {
  srand(time(0));

  size_t maxthreads = std::thread::hardware_concurrency();

  std::string result_file_name;
  size_t arg1;

  setup_logging();

  if (argc >= 2) {
    try {
      arg1 = ::std::stoul(args[1]);
    } catch (::std::invalid_argument const& e) {
      BOOST_LOG_TRIVIAL(error)
          << "Invalid arg1: expected positive number but got \"" << args[1]
          << "\"" << ::std::endl;
      return 1;
    } catch (::std::out_of_range const& e) {
      BOOST_LOG_TRIVIAL(error)
          << "Invalid arg1: value " << args[1] << " is too big" << ::std::endl;
      return 1;
    }
    if (arg1 == 0) {
      ::std::cerr << "Invalid arg1: expected positive number but got 0"
                  << ::std::endl;
      return 1;
    }

    if (argc >= 3) result_file_name = args[2];
  } else {
    arg1 = ::std::thread::hardware_concurrency();
    if (arg1 == 0) arg1 = 1;  // minimal value
  }

  {
    __sighandler_t sighandler = [](int const signal) {
      BOOST_LOG_TRIVIAL(info)
          << "Shutting down due to signal " << signal << std::endl;
      shutdown = signal;
      shutdown = true;
    };
    sysv_signal(SIGINT, sighandler);
    sysv_signal(SIGSTOP, sighandler);
    sysv_signal(SIGTERM, sighandler);
  }

  json result;

  vector<std::thread> threads(maxthreads);

  for (size_t nthr = 0; nthr < maxthreads; nthr++) {
    threads[nthr] = std::thread(write_to_json, std::ref(result));
  }

  for (size_t nthr = 0; nthr < maxthreads; nthr++) {
    threads[nthr].join();
  }

  std::ofstream outstr(result_file_name);
  if (outstr) {
    outstr << result;
  } else {
    BOOST_LOG_TRIVIAL(error) << "Error while writing report file";
  }

  std::cout << result;
  return 0;
}
