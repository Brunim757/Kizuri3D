#include "Application.h"

#include <atomic>
#include <caligo/core/JobSystem.h>
#include <caligo/core/Log.h>
#include <caligo/core/Result.h>
#include <caligo/core/Time.h>
#include <caligo/platform/File.h>
#include <caligo/platform/Input.h>
#include <caligo/platform/System.h>
#include <caligo/platform/Window.h>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace caligo {

namespace {

struct Options {
  bool headless = false;
  bool help = false;
  uint32_t frames = 0;
  LogLevel logLevel = LogLevel::Info;
};

std::optional<uint32_t> ParsePositiveInteger(std::string_view text) {
  uint32_t value = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const std::from_chars_result result = std::from_chars(begin, end, value);
  if (result.ec != std::errc{} || result.ptr != end || value == 0) {
    return std::nullopt;
  }
  return value;
}

std::optional<LogLevel> ParseLogLevel(std::string_view text) {
  if (text == "trace") {
    return LogLevel::Trace;
  }
  if (text == "debug") {
    return LogLevel::Debug;
  }
  if (text == "info") {
    return LogLevel::Info;
  }
  if (text == "warn") {
    return LogLevel::Warn;
  }
  if (text == "error") {
    return LogLevel::Error;
  }
  return std::nullopt;
}

Result<Options> ParseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--headless") {
      options.headless = true;
      continue;
    }
    if (argument == "--help" || argument == "-h") {
      options.help = true;
      continue;
    }
    if (argument == "--frames") {
      ++index;
      if (index >= argc) {
        return Error{"--frames requires a value", 1};
      }
      const std::optional<uint32_t> frames = ParsePositiveInteger(argv[index]);
      if (!frames.has_value()) {
        return Error{std::format("invalid frame count: {}", argv[index]), 2};
      }
      options.frames = *frames;
      continue;
    }
    if (argument == "--log-level") {
      ++index;
      if (index >= argc) {
        return Error{"--log-level requires a value", 3};
      }
      const std::optional<LogLevel> level = ParseLogLevel(argv[index]);
      if (!level.has_value()) {
        return Error{std::format("invalid log level: {}", argv[index]), 4};
      }
      options.logLevel = *level;
      continue;
    }
    return Error{std::format("unknown argument: {}", argument), 5};
  }
  return options;
}

void PrintUsage() {
  std::printf("Caligo\n\n");
  std::printf("Usage: caligo [options]\n\n");
  std::printf("  --headless           Run without a window\n");
  std::printf("  --frames <count>     Stop after the given number of frames\n");
  std::printf("  --log-level <level>  trace, debug, info, warn, error\n");
  std::printf("  --help               Show this message\n");
}

int RunHeadless(JobSystem& jobs, uint32_t frames) {
  constexpr uint32_t kWorkItems = 1024;
  constexpr uint32_t kMinRange = 64;
  constexpr uint64_t kExpectedPerFrame =
      static_cast<uint64_t>(kWorkItems - 1) * static_cast<uint64_t>(kWorkItems) / 2;

  FrameTimer timer;
  std::atomic<uint64_t> checksum{0};
  uint64_t ticks = 0;

  for (uint32_t frame = 0; frame < frames; ++frame) {
    const FrameStep step = timer.Advance(timer.FixedDelta());
    ticks += step.steps;
    checksum.store(0, std::memory_order_relaxed);
    jobs.ParallelFor(kWorkItems, kMinRange, [&checksum](uint32_t begin, uint32_t end) {
      uint64_t local = 0;
      for (uint32_t index = begin; index < end; ++index) {
        local += index;
      }
      checksum.fetch_add(local, std::memory_order_relaxed);
    });
    if (checksum.load(std::memory_order_relaxed) != kExpectedPerFrame) {
      CALIGO_LOG_ERROR("headless checksum failed at frame {}", frame);
      return 1;
    }
  }

  if (ticks != static_cast<uint64_t>(frames)) {
    CALIGO_LOG_ERROR("headless tick count mismatch: expected {}, got {}", frames, ticks);
    return 2;
  }

  CALIGO_LOG_INFO("headless run complete: {} frames, {} ticks", frames, ticks);
  return 0;
}

int RunWindowed(uint32_t frames) {
  platform::Window window;
  const Result<void> created =
      window.Create(platform::WindowDesc{.title = "Caligo", .width = 1280, .height = 720});
  if (!created) {
    CALIGO_LOG_ERROR("could not create window: {}", created.GetError().message);
    return 1;
  }

  Clock clock;
  FrameTimer timer;
  uint32_t frameCount = 0;
  uint64_t tickCount = 0;

  while (!window.ShouldClose()) {
    window.PollEvents();
    if (window.GetInput().State().WasKeyPressed(platform::Key::Escape)) {
      window.RequestClose();
    }

    const FrameStep step = timer.Advance(clock.DeltaSeconds());
    tickCount += step.steps;
    ++frameCount;

    if (frames > 0 && frameCount >= frames) {
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  window.Destroy();
  CALIGO_LOG_INFO("windowed run complete: {} frames, {} ticks", frameCount, tickCount);
  return 0;
}

}

int RunApplication(int argc, char** argv) {
  Result<Options> parsed = ParseOptions(argc, argv);
  if (!parsed) {
    std::fprintf(stderr, "error: %s\n", parsed.GetError().message.c_str());
    PrintUsage();
    return 1;
  }

  const Options options = parsed.Value();
  if (options.help) {
    PrintUsage();
    return 0;
  }

  platform::ConfigureProcess();
  Logger& logger = GetLogger();
  logger.SetMinimumLevel(options.logLevel);
  logger.AddSink(std::make_unique<ConsoleSink>());

  const Result<std::filesystem::path> executableDirectory = platform::ExecutableDirectory();
  if (executableDirectory) {
    const std::filesystem::path logPath = executableDirectory.Value() / "logs" / "caligo.log";
    auto fileSink = std::make_unique<FileSink>(logPath);
    if (fileSink->IsOpen()) {
      logger.AddSink(std::move(fileSink));
    } else {
      CALIGO_LOG_WARN("could not open log file {}", logPath.string());
    }
  } else {
    CALIGO_LOG_WARN("could not resolve executable directory: {}",
                    executableDirectory.GetError().message);
  }

  CALIGO_LOG_INFO("Caligo {} ({})", CALIGO_VERSION, CALIGO_CONFIGURATION);

  JobSystem jobs;
  jobs.Initialize();

  int result = 0;
  if (options.headless) {
    result = RunHeadless(jobs, options.frames == 0 ? 120 : options.frames);
  } else {
    result = RunWindowed(options.frames);
  }

  jobs.Shutdown();
  logger.Flush();
  return result;
}

}
