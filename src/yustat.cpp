#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <thread>
#include <memory>

#include <getopt.h>

bool StartsWith(const std::string& str, const std::string& prefix) {
  return str.rfind(prefix, 0) != std::string::npos;
}

bool EndsWith(const std::string& str, const std::string& suffix) {
  return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), std::string::npos, suffix) == 0;
}

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWith(str, prefix)) {
    return str.substr(prefix.size());
  }
  return str;
}

std::string RemoveSuffix(const std::string& str, const std::string& suffix) {
  if (EndsWith(str, suffix)) {
    return str.substr(0, str.size() - suffix.size());
  }
  return str;
}

std::string Trim(const std::string& str) {
  std::string::size_type p1 = str.find_first_not_of(" \t\n\r");
  if (p1 == std::string::npos) {
    return str;
  }
  std::string::size_type p2 = str.find_last_not_of(" \t\n\r");
  return str.substr(p1, p2 - p1);
}

template <typename T>
std::string to_percentage(T value, T total) {
  if (total == 0) return "  0.0%";
  double percent_value = 100.0 * value / total;
  std::ostringstream oss;
  oss << std::fixed << std::setw(5) << std::setprecision(1) << std::setfill(' ')
    << percent_value << "%";
  return oss.str();
}

std::string human_readable_time(double time) {
  time_t epoch = static_cast<time_t>(time);
  double subsec = time - epoch;
  int sec = epoch % 60;
  epoch /= 60;
  int min = epoch % 60;
  epoch /= 60;
  int hour = epoch & 24;
  epoch /= 24;
  int day = epoch;
  std::ostringstream oss;
  oss
    << day << "days "
    << std::setw(2) << std::setfill('0')
    << hour << ":"
    << std::setw(2) << std::setfill('0')
    << min << ":"
    << std::setw(2) << std::setfill('0')
    << sec;
  return oss.str();
}

std::string gen_bar(int width, int value, int total) {
  static const std::vector<std::string> kBlocks = {
    " ", "\u258f", "\u258e", "\u258d", "\u258c", "\u258b", "\u258a", "\u2589", "\u2588"
  };
  if (total == 0) total = 1;
  int tick = std::min(kBlocks.size() * width * value / total, kBlocks.size() * width - 1);
  int div = tick / kBlocks.size();
  int rem = tick % kBlocks.size();

  std::string bar;
  size_t i = 0;
  for (; i < div; ++i) {
    bar += *kBlocks.rbegin();
  }
  bar += kBlocks[rem];
  ++i;
  for (; i < width; ++i) {
    bar += *kBlocks.begin();
  }
  return bar;
}

std::string gen_vertical_bar(int value, int total) {
  static const std::vector<std::string> kBlocks = {
    " ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"
  };
  if (total == 0) total = 1;
  int idx = std::min(kBlocks.size() * value / total, kBlocks.size() - 1);
  return kBlocks[idx];
}

class Stat {
 public:
  enum class Format {
    kTMUX,
    kConsole,
    kJSON,
  };

  class Options {
   public:
    Format format = Format::kTMUX;
  };

  explicit Stat(const Options& options) : options_(options) {
    Update();
  }

  void Update() {
    UpdateUptime();
    UpdateMemory();
    UpdateCPU();
    UpdateLoadAverage();
    UpdateWallClock();
  }

  void Dump(std::ostream& os) const {
    switch (options_.format) {
      case Format::kTMUX:
        DumpTMUX(os);
        break;
      case Format::kConsole:
      case Format::kJSON:
        std::cerr << "Not implemented yet" << std::endl;
        break;
    }
  }

 private:
  void UpdateUptime() {
    std::ifstream ifs("/proc/uptime");
    ifs >> uptime_;
  }

  void UpdateMemory() {
    static const std::string kMemoryTotal = "MemTotal:";
    static const std::string kMemoryAvailable = "MemAvailable:";
    static const std::string kSwapTotal = "SwapTotal:";
    static const std::string kSwapFree = "SwapFree:";

    std::ifstream ifs("/proc/meminfo");
    std::string line;
    while (std::getline(ifs, line)) {
      if (StartsWith(line, kMemoryTotal)) {
        mem_total_ = std::stoull(Trim(RemoveSuffix(RemovePrefix(line, kMemoryTotal), "kB")));
      } else if (StartsWith(line, kMemoryAvailable)) {
        mem_available_ = std::stoull(Trim(RemoveSuffix(RemovePrefix(line, kMemoryAvailable), "kB")));
      } else if (StartsWith(line, kSwapTotal)) {
        swap_total_ = std::stoull(Trim(RemoveSuffix(RemovePrefix(line, kSwapTotal), "kB")));
      } else if (StartsWith(line, kSwapFree)) {
        swap_free_ = std::stoull(Trim(RemoveSuffix(RemovePrefix(line, kSwapFree), "kB")));
      }
    }
  }

  void UpdateCPU() {
    std::vector<time_t> busy_times, idle_times;
    {
      std::ifstream ifs("/proc/stat");
      std::string line;
      while (std::getline(ifs, line)) {
        if (!StartsWith(line, "cpu")) break;
        std::string name;
        std::istringstream iss(line);
        time_t user_time, nice_time, system_time, idle_time;
        iss >> name >> user_time >> nice_time >> system_time >> idle_time;
        busy_times.push_back(user_time + nice_time + system_time);
        idle_times.push_back(idle_time);
      }
    }

    // first time
    if (last_cpu_busy_times_.empty()) {
      last_cpu_busy_times_ = busy_times;
      last_cpu_idle_times_ = idle_times;
      cpu_usages_.resize(busy_times.size(), 0.0);
      return;
    }

    for (size_t i = 0; i < busy_times.size(); ++i) {
      time_t busy_diff = busy_times[i] - last_cpu_busy_times_[i];
      time_t idle_diff = idle_times[i] - last_cpu_idle_times_[i];
      time_t total_diff = busy_diff + idle_diff;

      double usage = (total_diff == 0) ? 0.0 : static_cast<double>(busy_diff) / total_diff * 100.0;
      cpu_usages_[i] = usage;

      last_cpu_busy_times_[i] = busy_times[i];
      last_cpu_idle_times_[i] = idle_times[i];
    }
  }

  void UpdateLoadAverage() {
    std::ifstream ifs("/proc/loadavg");
    ifs >> load1_ >> load5_ >> load15_ >> procs_;
  }

  void UpdateWallClock() {
    wall_clock_ = time(nullptr);
  }

  std::ostream& DumpUptime(std::ostream& os) const {
    os << human_readable_time(uptime_);
    return os;
  }
  std::ostream& DumpMemory(std::ostream& os, int bar_width = 8) const {
    os
      << to_percentage(mem_total_ - mem_available_, mem_total_)
      << "[" << gen_bar(bar_width, mem_total_ - mem_available_, mem_total_) << "]";
    return os;
  }
  std::ostream& DumpSwap(std::ostream& os, int bar_width = 8) const {
    os
      << to_percentage(swap_total_ - swap_free_, swap_total_)
      << "[" << gen_bar(bar_width, swap_total_ - swap_free_, swap_total_) << "]";
    return os;
  }
  std::ostream& DumpCPU(std::ostream& os, int bar_width = 8) const {
    os
      << to_percentage(cpu_usages_[0], 100.0)
      << "[" << gen_bar(bar_width, cpu_usages_[0], 100) << "]";

    os << "[";
    for (size_t i = 1; i < cpu_usages_.size(); ++i) {
      os << gen_vertical_bar(cpu_usages_[i], 100);
    }
    os << "]";
    return os;
  }
  std::ostream& DumpLoadAverage(std::ostream& os) const {
    os << load1_ << " " << load5_ << " " << load15_ << " " << procs_;
    return os;
  }
  std::ostream& DumpWallClock(std::ostream& os) const {
    struct tm tm_info;
    localtime_r(&wall_clock_, &tm_info);
    os << std::put_time(&tm_info, "%F(%a) %T");
    return os;
  }

  void DumpTMUX(std::ostream& os) const {
    // TODO: read format string from config
    const static std::string kFormatString =
      "#[fg=colour4]{uptime} "
      "#[fg=colour2]{load}"
      "#[fg=colour3]{memory}"
      "#[fg=colour5]{swap}"
      "#[fg=colour6]{cpu} "
      "#[fg=colour7]{wall_clock}";

    std::string::size_type pos, last_pos = 0;
    for (pos = kFormatString.find('{'); pos != std::string::npos; pos = kFormatString.find('{', last_pos)) {
      if (pos + 1 < kFormatString.size() && kFormatString[pos + 1] == '{') {
        // escaped {{
        os << kFormatString.substr(last_pos, pos - last_pos + 1);
        last_pos = pos + 2;
        continue;
      }

      std::string::size_type end_pos = kFormatString.find('}', pos);
      if (end_pos == std::string::npos) {
        std::cerr << "Error: unmatched '{' in format string" << std::endl;
        return;
      }
      os << kFormatString.substr(last_pos, pos - last_pos);
      std::string key = kFormatString.substr(pos + 1, end_pos - pos - 1);
      if (key == "uptime") {
        DumpUptime(os);
      } else if (key == "load") {
        DumpLoadAverage(os);
      } else if (key == "memory") {
        DumpMemory(os, 5);
      } else if (key == "swap") {
        DumpSwap(os, 5);
      } else if (key == "cpu") {
        DumpCPU(os, 5);
      } else if (key == "wall_clock") {
        DumpWallClock(os);
      } else {
        std::cerr << "Error: unknown key '" << key << "' in format string" << std::endl;
        return;
      }
      last_pos = end_pos + 1;
    }
  }

 private:
  Options options_;

  // uptime
  double uptime_;

  // memory
  size_t mem_total_;
  size_t mem_available_;
  size_t swap_total_;
  size_t swap_free_;

  // cpu
  // [cpu, cpu0, cpu1, ...]
  std::vector<time_t> last_cpu_busy_times_;
  std::vector<time_t> last_cpu_idle_times_;
  std::vector<double> cpu_usages_;

  // load average
  double load1_;
  double load5_;
  double load15_;
  std::string procs_;

  // wall clock
  time_t wall_clock_;
};

class Options {
 public:
  Options() : output_file(""), stat_options() {}

  std::string output_file;
  int interval_seconds = 0; // 0 means one-shot
  Stat::Options stat_options;
};

void Usage(std::ostream& os, int, char** argv) {
  os
    << "Usage: " << argv[0] << " [options]" << std::endl
    << "Options:" << std::endl
    << "  -h, --help              Show this help message and exit" << std::endl
    << "  -o, --output FILE       Output to FILE (default: STDOUT)" << std::endl
    << "  -i, --interval SECONDS  Update interval in seconds (default: one-shot)" << std::endl
    << "  -f, --format FORMAT     Output format (tmux, console, json; default: tmux)" << std::endl
    ;
}

class OutputStream : public std::ostream {
 public:
  explicit OutputStream(const std::string& filename)
    : filename_(filename), tmp_filename_(filename + ".tmp") {

    if (filename.empty() || filename == "-") {
      rdbuf(std::cout.rdbuf());
      is_file_stream_ = false;
    } else {
      file_stream_ = std::make_unique<std::ofstream>(tmp_filename_);
      if (!file_stream_->is_open()) {
        std::cerr << "Failed to open output file: " << filename << std::endl;
        exit(1);
      }
      rdbuf(file_stream_->rdbuf());
      is_file_stream_ = true;
    }
  }
  ~OutputStream() {
    if (is_file_stream_ && file_stream_) {
      file_stream_->close();
    }
  }

  OutputStream& flush() {
    std::ostream::flush();
    if (is_file_stream_ && file_stream_) {
      if (std::rename(tmp_filename_.c_str(), filename_.c_str()) != 0) {
        std::cerr << "Failed to rename temporary file to final output file: " << filename_ << std::endl;
        exit(1);
      }
    }
    return *this;
  }

 private:
  const std::string filename_;
  const std::string tmp_filename_;
  std::unique_ptr<std::ofstream> file_stream_;
  bool is_file_stream_;
};

int main(int argc, char** argv) {
  Options options;
  while (true) {
    static struct option long_options[] = {
      { "help", no_argument, nullptr, 'h' },
      { "output", required_argument, nullptr, 'o' },
      { "interval", required_argument, nullptr, 'i' },
      { "format", required_argument, nullptr, 'f' },
      { nullptr, 0, nullptr, 0 }
    };

    int opt = getopt_long(argc, argv, "hbo:i:f:", long_options, nullptr);
    if (opt == -1) break;

    switch (opt) {
      case 'h':
        Usage(std::cout, argc, argv);
        return 0;
      case 'o':
        options.output_file = optarg;
        break;
      case 'i':
        options.interval_seconds = std::stoi(optarg);
        break;
      case 'f':
        if (std::string(optarg) == "tmux") {
          options.stat_options.format = Stat::Format::kTMUX;
        } else if (std::string(optarg) == "console") {
          options.stat_options.format = Stat::Format::kConsole;
        } else if (std::string(optarg) == "json") {
          options.stat_options.format = Stat::Format::kJSON;
        } else {
          std::cerr << "Unknown format: " << optarg << std::endl;
          Usage(std::cerr, argc, argv);
          return 1;
        }
        break;
      default:
        Usage(std::cerr, argc, argv);
        return 1;
    }
  }

  Stat stat(options.stat_options);
  while (true) {
    OutputStream output_stream(options.output_file);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stat.Update();
    stat.Dump(output_stream);
    output_stream.flush();
    if (options.interval_seconds <= 0) break;
    std::this_thread::sleep_for(std::chrono::seconds(options.interval_seconds));
  }
  return 0; // This line will never be reached in the current implementation
}
