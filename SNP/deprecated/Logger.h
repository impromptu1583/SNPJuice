#pragma once
#include "Common.h"

inline bool g_fatal = false;

static constexpr auto FileDateFormat = "{:%F_%Hh_%Mm}";
static constexpr auto LogDateFormat = "{:%H:%M:%OS}";

#define AnsiWhite   "\033[37m"
#define AnsiYellow  "\033[33m"
#define AnsiPurple  "\033[35m"
#define AnsiCyan    "\033[36m"
#define AnsiRed     "\033[31m"
#define AnsiBoldRed "\033[91m"
#define AnsiReset   "\033[0m"

enum class LogLevel {
    None,
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
    Trace
};

inline std::string to_string(LogLevel log_level) {
    switch (log_level) {
        EnumStringCase(LogLevel::None);
        EnumStringCase(LogLevel::Fatal);
        EnumStringCase(LogLevel::Error);
        EnumStringCase(LogLevel::Warn);
        EnumStringCase(LogLevel::Info);
        EnumStringCase(LogLevel::Debug);
        EnumStringCase(LogLevel::Trace);
    }
    return std::to_string((s32)log_level);
}

NLOHMANN_JSON_SERIALIZE_ENUM(LogLevel, {
    {LogLevel::None, nullptr},
    {LogLevel::Fatal, "fatal"},
    {LogLevel::Error, "error"},
    {LogLevel::Warn, "warn"},
    {LogLevel::Info, "info"},
    {LogLevel::Debug, "debug"},
    {LogLevel::Trace, "trace"},
});

inline std::mutex g_cout_mutex;

class LogFile {
public:
    LogFile(std::string_view name = "log") {
        fs::path dir_path = g_starcraft_dir / "crownlink_logs";
        std::error_code ec;
        fs::create_directory(dir_path, ec);

        std::stringstream ss;
        ss << name << "_" << std::format(FileDateFormat, std::chrono::zoned_time{
            std::chrono::current_zone(),std::chrono::system_clock::now() });
        m_out.open(dir_path / ss.str(), std::ios::app);
    }

    [[nodiscard]] std::lock_guard<std::mutex> lock() {
        return std::lock_guard{m_mutex};
    }

    friend LogFile& operator<<(LogFile& out, const auto& value) {
        out.m_out << value;
        return out;
    }

    typedef std::ostream& StdStreamCallback(std::ostream&);

    friend LogFile& operator<<(LogFile& out, StdStreamCallback* callback) {
        callback(out.m_out);
        return out;
    }

private:
    std::mutex m_mutex;
    std::ofstream m_out;
};

class Logger {
public:
    Logger() = default;

    Logger(std::convertible_to<std::string> auto&&... prefixes)
        : m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    Logger(LogFile* log_file, std::convertible_to<std::string> auto&&... prefixes)
        : m_log_file{log_file}, m_prefixes{std::forward<decltype(prefixes)>(prefixes)...} {
    }

    Logger(const Logger& logger, std::convertible_to<std::string> auto&&... prefixes)
        : m_log_file{logger.m_log_file}, m_prefixes{logger.m_prefixes} {
        (m_prefixes.emplace_back(prefixes), ...);
    }

    void fatal(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Fatal) return;
        const auto message = std::vformat(format, std::make_format_args(args...));
        log(std::cerr, "Fatal", AnsiBoldRed, message);

        MessageBoxA(0, message.c_str(), "CrownLink Fatal Error", MB_ICONERROR | MB_OK);
        g_fatal = true;
    }

    void error(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Error) return;
        log(std::cerr, "Error", AnsiRed, std::vformat(format, std::make_format_args(args...)));
    }

    void warn(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Warn) return;
        log(std::cout, "Warn", AnsiYellow, std::vformat(format, std::make_format_args(args...)));
    }

    void info(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Info) return;
        log(std::cout, "Info", AnsiWhite, std::vformat(format, std::make_format_args(args...)));
    }

    void debug(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Debug) return;
        log(std::cerr, "Debug", AnsiPurple, std::vformat(format, std::make_format_args(args...)));
    }

    void trace(std::string_view format, const auto&... args) {
        if (s_log_level < LogLevel::Trace) return;
        log(std::cerr, "Trace", AnsiCyan, std::vformat(format, std::make_format_args(args...)));
    }

    static void set_log_level(LogLevel log_level) { s_log_level = log_level; }

    static Logger& root() {
		static LogFile file{"CrownLink"};
		static Logger logger{&file};
        return logger;
    }

private:
    void log(std::ostream& out, std::string_view log_level, std::string_view ansi_color, std::string_view string) {
        const auto prefix = make_prefix(log_level);

		// std::endl flushes, which is inteded for logger
        {
            std::lock_guard lock{g_cout_mutex};
			out << ansi_color << prefix << string << AnsiReset << std::endl; 
        }
        if (m_log_file) {
            const auto lock = m_log_file->lock();
            *m_log_file << prefix << string << std::endl;
        }
    }

    std::string make_prefix(std::string_view log_level) {
        std::stringstream ss;
        const auto current_time = time(nullptr);
        ss << "[" << std::format(LogDateFormat, std::chrono::utc_clock::now()) << " " << log_level << "]";
        for (const std::string& prefix : m_prefixes) {
            ss << " " << prefix;
        }
        ss << ": ";
        return ss.str();
    }

private:
    std::vector<std::string> m_prefixes;
    LogFile* m_log_file = nullptr;
    inline static LogLevel s_log_level = LogLevel::Info;
};