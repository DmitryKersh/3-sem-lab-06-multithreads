// Copyright 2020 Your Name <your_email>

#include <header.hpp>

mutex mtx;

static ::std::atomic_bool shutdown = false;


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

json find_prototype(const std::string& suffix) {
  json j;
  std::string data_string = random_hex_string();

  while (!is_appropriate(picosha2::hash256_hex_string(data_string), suffix)) {
    data_string = random_hex_string();
    if (shutdown){
      return  json("shutdown");
    }
  }

  j["timestamp"] = std::to_string(clock()) + " (" +
                   std::to_string(clock() / 1'000'000.0) + " s.)";
  j["hash"] = picosha2::hash256_hex_string(data_string);
  j["data"] = data_string;

  return j;
}

void write_to_json(json& output, size_t thread_number){
  json j = find_prototype("0000");
  while (!shutdown) {
    if (j == json("shutdown")) {
      return;
    }

    mtx.lock();
    j["thread"] = thread_number;
    output.push_back(j);
    mtx.unlock();
    j = find_prototype("0000");
  }
}


int main(/*int argc, char* argv[]*/) {
  srand(time(0));
  /*{
    __sighandler_t sighandler = [](int const signal){
      BOOST_LOG_TRIVIAL(info) << "Shutting down due to signal " << signal << std::endl;
      shutdown = true;
    };
    sysv_signal(SIGINT, sighandler);
    sysv_signal(SIGSTOP, sighandler);
    sysv_signal(SIGTERM, sighandler);
  }*/
  size_t maxthreads = std::thread::hardware_concurrency();

  json result;

  vector<std::thread> threads(maxthreads);

  for (size_t nthr = 0; nthr < maxthreads; nthr++){
    threads[nthr] = std::thread(write_to_json, std::ref(result), nthr);
  }
  while (clock() < 20'000'000){
    sleep(1);
  }
  shutdown = true;
  for (size_t nthr = 0; nthr < maxthreads; nthr++){
    threads[nthr].join();
  }
  std::cout << result;
  return 0;
}
