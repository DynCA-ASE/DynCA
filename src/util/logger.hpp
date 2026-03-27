#pragma once

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <unistd.h>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>

namespace logger {
enum class Color {
    // clang-format off
    BLACK     = 30,
    RED       = 31,
    GREEN     = 32,
    YELLOW    = 33,
    BLUE      = 34,
    MAGENTA   = 35,
    CYAN      = 36,
    WHITE     = 37,
    RESET     = 39,
    // clang-format on
};
}

template <>
struct fmt::formatter<logger::Color> : fmt::formatter<std::string> {
    template <typename FormatContext>
    constexpr auto format(logger::Color c, FormatContext &ctx) const -> decltype(ctx.out()) {
        return formatter<std::string>::format(fmt::format("\033[{}m", static_cast<int>(c)), ctx);
    }
};

namespace logger {
namespace details {
template <size_t N>
consteval inline std::array<char, N * 3> get_fstring_impl() {
    std::array<char, N * 3> str{};
    for (size_t i = 0; i < N; ++i) {
        str[i * 3] = '{';
        str[i * 3 + 1] = '}';
        str[i * 3 + 2] = ' ';
    }
    str[N * 3 - 1] = '\0';
    return str;
}

template <size_t N>
struct fstring {
    static constexpr auto value = get_fstring_impl<N>();
};

template <size_t N>
inline consteval std::string_view get_fstring() {
    return std::string_view(fstring<N>::value.begin(), fstring<N>::value.end() - 1);
}
}  // namespace details

enum class Verbosity { QUIET, NORMAL, VERBOSE };

inline bool with_time = true;
inline auto start_time = std::chrono::high_resolution_clock::now();
inline void reset_time() { start_time = std::chrono::high_resolution_clock::now(); }

namespace details {
/// wrap a streambuf. remove all the escape codes
class wrapper_filebuf : public std::streambuf {
 public:
    wrapper_filebuf() : sb_(nullptr) {}
    explicit wrapper_filebuf(std::streambuf *sb) : sb_(sb) {}

 private:
    int_type overflow(int_type c) override {
        if (escape_) {
            if (c == 'm') escape_ = false;
        } else {
            if (c != '\033') return sb_->sputc(c);
            escape_ = true;
        }
        return c;
    }

    int sync() override { return sb_->pubsync(); }

 private:
    std::streambuf *sb_;
    bool escape_ = false;
};

/// a streambuf which always does nothing.
struct none_streambuf : std::streambuf {
    int_type overflow(int_type c) override { return c; }
    int sync() override { return 0; }
};
static inline none_streambuf NONE_STREAMBUF;

/// There are three modes depend on `verbose`.
class Logger {
    friend class wrapper_stdout;

    /// the original stdout streambuf
    inline static std::streambuf *const STDOUT_BUF_ = std::cout.rdbuf();
    /// the original stderr streambuf
    inline static std::streambuf *const STDERR_BUF_ = std::cerr.rdbuf();

    static constexpr size_t DETAIL_LINE_LIMIT = 6;
    static constexpr size_t DETAIL_MAX_LENGTH = 80;
    static constexpr std::string_view LINE_SEP = "│";
    static constexpr std::string_view TITLE = "┌───────── DETAIL LOG ────────────────────────\n";

 public:
    Logger() {
        log_file_streambuf_ = wrapper_filebuf(log_file_.rdbuf());
        if (isatty(fileno(stdout))) {
            stdout_buf_ = STDOUT_BUF_;
            pure_buf_ = wrapper_filebuf(&NONE_STREAMBUF);
        } else {
            stdout_buf_ = &NONE_STREAMBUF;
            pure_buf_ = wrapper_filebuf(STDOUT_BUF_);
        }
    }
    ~Logger() { flush_info(); }

    /// the verbosity mode
    Verbosity verbose = Verbosity::NORMAL;
    /// the level of current print message
    Verbosity level = Verbosity::NORMAL;

    inline void open_log_file(const std::string &filename) { log_file_.open(filename); }

    template <typename... Ts>
    inline void print(fmt::format_string<Ts...> fmt, Ts &&...args) {
        print_impl(fmt::format(std::move(fmt), std::forward<Ts>(args)...));
    }

    template <typename... Ts>
    inline void println(fmt::format_string<Ts...> fmt, Ts &&...args) {
        print_impl(fmt::format(std::move(fmt), std::forward<Ts>(args)...));
        putc_impl(level, '\n');
    }

 private:
    // buffer for infomation
    std::string buffer_info_;
    // buffer for detail outputs
    std::string buffer_detail_;
    /// the log file
    std::ofstream log_file_;
    /// the default stdout buf
    std::streambuf *stdout_buf_;
    /// the pure stdout buf
    wrapper_filebuf pure_buf_;
    /// the log file wrapped streambuf
    wrapper_filebuf log_file_streambuf_;
    /// detail block lines
    std::deque<std::string> detail_lines_;

    inline void print_impl(const std::string &str) {
        if (level <= verbose) {
            for (auto c : str) putc_info(c);
        } else {
            for (auto c : str) putc_detail(c);
        }
    }

    inline void putc_impl(Verbosity level, char c) {
        return level <= verbose ? putc_info(c) : putc_detail(c);
    }

    inline void putc_info(char c) {
        buffer_info_.push_back(c);
        if (c == '\n') flush_info();
    }

    inline void putc_detail(char c) {
        buffer_detail_.push_back(c);
        if (c == '\n') flush_detail();
    }

    inline void flush_info() {
        flush_detail();
        if (buffer_info_.empty()) return;
        std::string buf = with_time ? get_time_string() + buffer_info_ : buffer_info_;
        buffer_info_.clear();
        if (buf.back() != '\n') buf.push_back('\n');
        log_file_streambuf_.sputn(buf.c_str(), buf.size());
        log_file_streambuf_.pubsync();
        if (verbose != Verbosity::QUIET) {
            pure_buf_.sputn(buf.c_str(), buf.size());
            pure_buf_.pubsync();
            int old_size = detail_lines_.size();
            detail_lines_.clear();
            if (old_size != 0) reflush_detail_block(old_size + 1);
            stdout_buf_->sputn(buf.c_str(), buf.size());
            stdout_buf_->pubsync();
        }
    }

    inline void flush_detail() {
        if (buffer_detail_.empty()) return;
        std::string buf = with_time ? get_time_string() + buffer_detail_ : buffer_detail_;
        buffer_detail_.clear();
        if (buf.back() != '\n') buf.push_back('\n');
        log_file_streambuf_.sputn(buf.c_str(), buf.size());
        log_file_streambuf_.pubsync();
        if (verbose != Verbosity::QUIET) {
            pure_buf_.sputn(buf.c_str(), buf.size());
            pure_buf_.pubsync();
            if (buf.size() >= DETAIL_MAX_LENGTH) {
                buf.resize(DETAIL_MAX_LENGTH - 4);
                buf += "...\n";
            }
            int old_size = detail_lines_.size();
            detail_lines_.push_back(std::move(buf));
            if (detail_lines_.size() > DETAIL_LINE_LIMIT) {
                detail_lines_.pop_front();
                reflush_detail_block(old_size);
            } else {
                if (old_size == 0) stdout_buf_->sputn(TITLE.begin(), TITLE.size());
                auto &line = detail_lines_.back();
                stdout_buf_->sputn(LINE_SEP.begin(), LINE_SEP.size());
                stdout_buf_->sputn(line.c_str(), line.size());
                stdout_buf_->pubsync();
            }
        }
    }

    inline void reflush_detail_block(int old_size) {
        if (old_size != 0) {
            std::string move_up = fmt::format("\e[{}A", old_size);
            stdout_buf_->sputn(move_up.c_str(), move_up.size());
            constexpr std::string_view clear = "\e[0J";
            stdout_buf_->sputn(clear.begin(), clear.size());
        }
        if (detail_lines_.empty()) return;
        for (auto &str : detail_lines_) {
            stdout_buf_->sputn(LINE_SEP.begin(), LINE_SEP.size());
            stdout_buf_->sputn(str.c_str(), str.size());
        }
        stdout_buf_->pubsync();
    }

    inline std::string get_time_string() {
        auto dura = std::chrono::duration_cast<std::chrono::duration<double>>(
            std::chrono::high_resolution_clock::now() - start_time);
        return fmt::format(" {:.3f}  ", dura.count());
    }

    inline int sync() {
        int r1 = stdout_buf_->pubsync();
        int r2 = log_file_streambuf_.pubsync();
        return r1 == 0 && r2 == 0 ? 0 : -1;
    }
};
}  // namespace details

inline details::Logger console{};

namespace details {
/// the output will redirect to both console (with level verbose) and log file (without color)
class wrapper_stdout : public std::streambuf {
    int_type overflow(int_type c) override {
        if (c == traits_type::eof()) return 0;
        console.putc_impl(Verbosity::VERBOSE, c);
        return c;
    }

    int sync() override { return console.sync(); }
};
inline details::wrapper_stdout wsb;
inline static int init_wsb__ = [] {
    std::cout.rdbuf(&wsb);
    // std::cerr.rdbuf(&wsb);
    return 0;
}();
}  // namespace details

template <typename... Ts>
inline void infof(Color c, fmt::format_string<Ts...> fmt, Ts &&...args) {
    console.print("{}", c);
    console.print(std::move(fmt), std::forward<Ts>(args)...);
    console.println("{}", Color::RESET);
}

template <typename... Ts>
inline void info(Color c, Ts &&...args) {
    infof(c, details::get_fstring<sizeof...(Ts)>(), std::forward<Ts>(args)...);
}

template <typename... Ts>
inline void infof(fmt::format_string<Ts...> fmt, Ts &&...args) {
    console.println(std::move(fmt), std::forward<Ts>(args)...);
}

template <typename... Ts>
inline void info(Ts &&...args) {
    infof(details::get_fstring<sizeof...(Ts)>(), std::forward<Ts>(args)...);
}

template <typename... Ts>
inline void debugf(Color c, fmt::format_string<Ts...> fmt, Ts &&...args) {
    console.level = Verbosity::VERBOSE;
    infof(c, std::move(fmt), std::forward<Ts>(args)...);
    console.level = Verbosity::NORMAL;
}

template <typename... Ts>
inline void debug(Color c, Ts &&...args) {
    console.level = Verbosity::VERBOSE;
    info(c, std::forward<Ts>(args)...);
    console.level = Verbosity::NORMAL;
}

template <typename... Ts>
inline void debugf(fmt::format_string<Ts...> fmt, Ts &&...args) {
    console.level = Verbosity::VERBOSE;
    infof(std::move(fmt), std::forward<Ts>(args)...);
    console.level = Verbosity::NORMAL;
}

template <typename... Ts>
inline void debug(Ts &&...args) {
    console.level = Verbosity::VERBOSE;
    info(std::forward<Ts>(args)...);
    console.level = Verbosity::NORMAL;
}

/// run command in bash and redirect output to std::cout
inline int exec_with_cout(const char *command) {
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        throw std::runtime_error(fmt::format("popen error: {}", errno));
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::cout << buf;
    }
    return pclose(pipe);
}
}  // namespace logger
