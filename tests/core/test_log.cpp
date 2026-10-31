#include <caligo/core/Log.h>
#include <doctest/doctest.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

class MemorySink final : public caligo::LogSink {
public:
  void Write(caligo::LogLevel, std::string_view line) override {
    m_lines.emplace_back(line);
  }

  void Flush() override {
    ++m_flushCount;
  }

  [[nodiscard]] const std::vector<std::string>& Lines() const noexcept {
    return m_lines;
  }

  [[nodiscard]] int FlushCount() const noexcept {
    return m_flushCount;
  }

private:
  std::vector<std::string> m_lines;
  int m_flushCount = 0;
};

}

TEST_CASE("logger writes enabled levels to every sink") {
  caligo::Logger logger;
  auto sink = std::make_unique<MemorySink>();
  MemorySink* memory = sink.get();
  logger.AddSink(std::move(sink));

  logger.SetMinimumLevel(caligo::LogLevel::Info);
  CHECK_FALSE(logger.IsEnabled(caligo::LogLevel::Debug));
  CHECK(logger.IsEnabled(caligo::LogLevel::Info));
  CHECK(logger.IsEnabled(caligo::LogLevel::Error));

  logger.Write(caligo::LogLevel::Debug, "hidden");
  logger.Write(caligo::LogLevel::Info, "visible");
  logger.Write(caligo::LogLevel::Error, "failure");
  logger.Flush();

  REQUIRE(memory->Lines().size() == 2);
  CHECK(memory->Lines()[0].find("visible") != std::string::npos);
  CHECK(memory->Lines()[0].find("[INFO]") != std::string::npos);
  CHECK(memory->Lines()[1].find("failure") != std::string::npos);
  CHECK(memory->Lines()[1].find("[ERROR]") != std::string::npos);
  CHECK(memory->FlushCount() == 1);
}

TEST_CASE("log level names are stable") {
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Trace)) == "TRACE");
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Debug)) == "DEBUG");
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Info)) == "INFO");
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Warn)) == "WARN");
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Error)) == "ERROR");
  CHECK(std::string(caligo::ToString(caligo::LogLevel::Fatal)) == "FATAL");
}
