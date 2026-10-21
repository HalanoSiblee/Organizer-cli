#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>
#if !defined(_WIN32)
#  include <unistd.h>
#  include <cstdlib>
#endif
namespace fs = std::filesystem;
static inline std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s)out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

static inline std::string_view ext_of(std::string_view filename) {
    if (filename.empty() || filename.front() == '.')return {
    };
    auto pos = filename.rfind('.');
    if (pos == std::string_view::npos || pos + 1 >= filename.size())return {
    };
    return filename.substr(pos + 1);
}

static inline bool is_hidden(std::string_view filename) {
    return!filename.empty() && filename.front() == '.';
}

static bool glob_match(std::string_view pat, std::string_view s) {
    std::size_t p = 0, si = 0, star_p = std::string_view::npos, star_s = 0;
    auto lower = [](char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    while (si < s.size()) {
        if (p < pat.size() && pat[p] == '*') {
            star_p = p++;
            star_s = si;
        } else if (p < pat.size() && pat[p] == '?') {
            ++p;
            ++si;
        } else if (p < pat.size() && pat[p] == '[') {
            std::size_t q = p + 1;
            bool neg = false;
            if (q < pat.size() && (pat[q] == '!' || pat[q] == '^')) {
                neg = true;
                ++q;
            }
            bool matched = false;
            char sc = lower(s[si]);
            while (q < pat.size() && pat[q] != ']') {
                if (q + 2 < pat.size() && pat[q + 1] == '-' && pat[q + 2] != ']') {
                    char lo = lower(pat[q]), hi = lower(pat[q + 2]);
                    if (sc >= lo && sc <= hi)matched = true;
                    q += 3;
                } else {
                    if (sc == lower(pat[q]))matched = true;
                    ++q;
                }
            }
            if (q >= pat.size())return false;
            if (matched == neg)return false;
            p = q + 1;
            ++si;
        } else if (p < pat.size() && lower(pat[p]) == lower(s[si])) {
            ++p;
            ++si;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            si = ++star_s;
        } else return false;
    } while (p < pat.size() && pat[p] == '*')++p;
    return p == pat.size();
}

struct YMD {
    int year;
    unsigned month;
    unsigned day;
};
static YMD local_ymd(fs::file_time_type ft) {
    using namespace std::chrono;
    static const auto file_now = fs::file_time_type::clock::now();
    static const auto sys_now = system_clock::now();
    auto sys_tp = sys_now + duration_cast<system_clock::duration>(ft - file_now);
    std::time_t t = system_clock::to_time_t(time_point_cast<system_clock::duration>(sys_tp));
    std::tm tm {
    };
#if defined(_WIN32)
    localtime_s( &tm, &t);
#else
    localtime_r( &t, &tm);
#endif
    return {
        tm.tm_year + 1900, static_cast<unsigned>(tm.tm_mon + 1), static_cast<unsigned>(tm.tm_mday)
    };
}

static inline std::string two_digits(unsigned v) {
    std::string s = "00";
    s[0] = static_cast<char>('0' + (v / 10) % 10);
    s[1] = static_cast<char>('0' + v % 10);
    return s;
}

static std::string human_bytes(std::uintmax_t b) {
    const char *units[] = {
        "B", "KiB", "MiB", "GiB", "TiB"
    };
    double v = static_cast<double>(b);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    char buf[32];
    if (u == 0)std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)b);
    else if (u == 1)std::snprintf(buf, sizeof(buf), "%.0f %s", v, units[u]);
    else std::snprintf(buf, sizeof(buf), "%.1f %s", v, units[u]);
    return buf;
}

static const std::unordered_set<std::string> kSkipExt = {
    "tmp", "part", "crdownload", "download", "partial", "lnk", "ini"
};
static const std::unordered_set<std::string> kSkipNameLower = {
    "thumbs.db", "desktop.ini"
};
static bool is_dangerous_dir(const fs::path & p) {
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(p, ec);
    if (ec)canon = p;
    std::string s = canon.string();
    while (s.size() > 1 && s.back() == '/')s.pop_back();
    static const char *kRoots[] = {
        "/", "/etc", "/usr", "/var", "/bin", "/sbin", "/lib", "/lib64", "/boot", "/dev", "/proc", "/sys", "/root"
    };
    for (auto *r : kRoots)if (s == r)return true;
#if !defined(_WIN32)
    if (const char *home = std::getenv("HOME"); home && *home) {
        std::string h = home;
        while (h.size() > 1 && h.back() == '/')h.pop_back();
        if (s == h)return true;
    }
#endif
    return false;
}

struct Options {
    bool do_year = false;
    bool do_month = false;
    bool do_day = false;
    bool do_time = false;
    bool dry_run = false;
    bool verbose = false;
    bool quiet = false;
    bool force = false;
    bool assume_yes = false;
    bool lowercase_ext = true;
    bool no_skip_lists = false;
    fs::path source = fs::current_path();
    fs::path target = fs::current_path();
    std::unordered_set<std::string> ext_filter;
    bool has_filter = false;
    std::vector<std::string> exclude_globs;
};
struct Layout {
    bool inc_ext = false;
    bool inc_year = false;
    bool inc_month = false;
    bool inc_day = false;
};
static Layout derive_layout(const Options & o) {
    int depth = 0;
    if (o.do_year)depth = std::max(depth, 1);
    if (o.do_month)depth = std::max(depth, 2);
    if (o.do_day)depth = std::max(depth, 3);
    if (depth == 0 && o.do_time)depth = 2;
    Layout L;
    L.inc_ext = !o.do_time;
    L.inc_year = depth >= 1;
    L.inc_month = depth >= 2;
    L.inc_day = depth >= 3;
    return L;
}

static std::string layout_desc(const Layout & L) {
    std::string s;
    if (L.inc_ext)s += "<ext>/";
    if (L.inc_year) {
        s += "<year>/";
        if (L.inc_month) {
            s += "<month>/";
            if (L.inc_day)s += "<day>/";
        }
    }
    s += "<file>";
    return s;
}

static void print_usage(const char *argv0) {
    std::cout << "Usage: " << argv0 << " [options]\n\n" << "Organize files in the source directory (top-level only).\n\n" << "Modes (choose one):\n" << "  (default)          <ext>/<file>\n" << "  --year             <ext>/<year>/<file>\n" << "  --month            <ext>/<year>/<month>/<file>   (implies --year)\n" << "  --day              <ext>/<year>/<month>/<day>/<file>\n" << "                     (implies --year and --month)\n" << "  --time             <year>/<month>/<file>   (no extension dir)\n" << "  --time --year      <year>/<file>\n" << "  --time --day       <year>/<month>/<day>/<file>\n\n" << "Options:\n" << "  -t                     Explicit type-only mode (same as default).\n" << "      --year             Include year directory.\n" << "      --month            Include year and month directories.\n" << "      --day              Include year, month and day directories.\n" << "      --time             Date-only layout (drops extension dir).\n" << "  -e <ext>               Only process this extension (repeatable).\n" << "                         Dot optional, case-insensitive (e.g. -e jpg).\n" << "      --exclude <glob>   Skip filenames matching glob (repeatable).\n" << "      --no-skip-lists    Do not apply built-in skip extensions/names.\n" << "      --source <dir>     Where to read files from (default: CWD).\n" << "      --target <dir>     Where to build the organized tree (default: source).\n" << "      --output <dir>     Alias for --target.\n" << "  -n, --dry-run          Show what would happen; move nothing.\n" << "  -v, --verbose          Log every move.\n" << "  -q, --quiet            Suppress summary and confirmation prompt.\n" << "  -y, --yes              Skip the interactive confirmation.\n" << "      --force            Bypass the root/danger-path safety guard.\n" << "      --no-lowercase-ext Keep original extension case in dir name.\n" << "  -h, --help             Show this help.\n";
}

static bool parse_args(int argc, char * *argv, Options & o, std::string & err) {
    auto need_value = [ & ](int &i, std::string_view flag)->const char * {
        if (i + 1 >= argc) {
            err = std::string(flag) + " requires a value";
            return nullptr;
        }
        return argv[ ++i];
    };
    bool target_set = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "-t") {
        } else if (a == "--year")o.do_year = true;
        else if (a == "--month")o.do_month = true;
        else if (a == "--day")o.do_day = true;
        else if (a == "--time")o.do_time = true;
        else if (a == "-n" || a == "--dry-run")o.dry_run = true;
        else if (a == "-v" || a == "--verbose")o.verbose = true;
        else if (a == "-q" || a == "--quiet")o.quiet = true;
        else if (a == "-y" || a == "--yes")o.assume_yes = true;
        else if (a == "--force")o.force = true;
        else if (a == "--no-lowercase-ext")o.lowercase_ext = false;
        else if (a == "--no-skip-lists")o.no_skip_lists = true;
        else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (a == "-e") {
            const char *v = need_value(i, a);
            if (!v)return false;
            std::string_view sv = v;
            if (!sv.empty() && sv.front() == '.')sv.remove_prefix(1);
            if (sv.empty()) {
                err = "-e requires a non-empty extension";
                return false;
            }
            o.ext_filter.insert(to_lower(sv));
            o.has_filter = true;
        } else if (a == "--exclude") {
            const char *v = need_value(i, a);
            if (!v)return false;
            if ( *v == '\0') {
                err = "--exclude requires a non-empty glob";
                return false;
            }
            o.exclude_globs.emplace_back(v);
        } else if (a == "--source") {
            const char *v = need_value(i, a);
            if (!v)return false;
            o.source = v;
        } else if (a == "--target" || a == "--output") {
            const char *v = need_value(i, a);
            if (!v)return false;
            o.target = v;
            target_set = true;
        } else if (!a.empty() && a.front() == '-' && a.size() > 1) {
            err = "unknown option: " + std::string(a);
            return false;
        } else {
            err = "unexpected positional argument: " + std::string(a);
            return false;
        }
    }
    if (!target_set)o.target = o.source;
    return true;
}

static bool safety_guard(const Options & o, const fs::path & src) {
    if (o.force)return true;
#if !defined(_WIN32)
    if (::getuid() == 0) {
        std::cerr << "[refuse] running as root.\n" << "         Files moved by this process will be owned by root and\n" << "         normal users may not be able to access them.\n" << "         Re-run as your normal user, or pass --force to override.\n";
        return false;
    }
#endif
    if (is_dangerous_dir(src)) {
        std::cerr << "[refuse] refusing to operate on a sensitive directory: " << src.string() << "\n" << "         Pass --force to override (you probably don't want to).\n";
        return false;
    }
    return true;
}

static bool confirm_proceed(const Options & o, const Layout & L) {
    if (o.dry_run || o.quiet || o.assume_yes)return true;
#if defined(_WIN32)
    return false;
#else
    if (!::isatty(::fileno(stdin)))return false;
#endif
    std::cout << "Mode  : " << layout_desc(L) << '\n' << "Source: " << o.source.string() << '\n' << "Target: " << o.target.string() << '\n' << "Proceed with moving files? [y/N] " << std::flush;
    std::string line;
    if (!std::getline(std::cin, line))return false;
    auto is_ws = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };
    std::size_t a = 0, b = line.size();
    while (a < b && is_ws(line[a]))++a;
    while (b > a && is_ws(line[b - 1]))--b;
    std::string_view ans(line.data() + a, b - a);
    return ans == "y" || ans == "Y" || ans == "yes" || ans == "YES" || ans == "Yes";
}

struct Stats {
    std::size_t moved = 0;
    std::uintmax_t moved_bytes = 0;
    std::size_t skipped_hidden = 0;
    std::size_t skipped_no_ext = 0;
    std::size_t skipped_filter = 0;
    std::size_t skipped_builtin = 0;
    std::size_t skipped_exclude = 0;
    std::size_t conflicts = 0;
    std::size_t errors = 0;
};
static void log_conflict(const fs::path & src, const fs::path & dst) {
    std::cerr << "[conflict] " << src.filename().string() << "  ->  " << dst.string() << " (exists, skipped)\n";
}

static void log_error(const fs::path & src, const std::error_code & ec) {
    std::cerr << "[error] " << src.filename().string() << ": " << ec.message() << '\n';
}

int main(int argc, char * *argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    Options opt;
    std::string parse_err;
    if (!parse_args(argc, argv, opt, parse_err)) {
        std::cerr << "error: " << parse_err << "\n\n";
        print_usage(argv[0]);
        return 2;
    }
    std::error_code ec;
    if (!fs::is_directory(opt.source, ec)) {
        std::cerr << "error: source is not a directory: " << opt.source.string();
        if (ec)std::cerr << " (" << ec.message() << ")";
        std::cerr << '\n';
        return 2;
    }
    if (!safety_guard(opt, opt.source))return 3;
    fs::create_directories(opt.target, ec);
    if (ec) {
        std::cerr << "error: cannot create target directory " << opt.target.string() << ": " << ec.message() << '\n';
        return 2;
    }
    fs::path target_canon = fs::weakly_canonical(opt.target, ec);
    if (ec)target_canon = opt.target;
    const Layout L = derive_layout(opt);
    if (!confirm_proceed(opt, L)) {
        std::cerr << "aborted.\n";
        return 3;
    }
    Stats st;
    std::error_code iter_ec;
    std::string last_ext, last_year, last_month, last_day;
    fs::path last_parent;
    fs::directory_iterator it(opt.source, fs::directory_options::skip_permission_denied, iter_ec);
    if (iter_ec) {
        std::cerr << "error: cannot open " << opt.source.string() << ": " << iter_ec.message() << '\n';
        return 1;
    }
    const fs::path & base = opt.target;
    for (const auto &entry : it) {
        std::error_code e;
        if (!entry.is_regular_file(e))continue;
        const fs::path & path = entry.path();
        if (opt.target != opt.source) {
            std::error_code ce;
            fs::path ep = fs::weakly_canonical(path, ce);
            if (!ce && ep == target_canon)continue;
        }
        const std::string fname = path.filename().string();
        if (is_hidden(fname)) {
            ++st.skipped_hidden;
            continue;
        }
        bool excluded = false;
        for (const auto &g : opt.exclude_globs)if (glob_match(g, fname)) {
            excluded = true;
            break;
        }
        if (excluded) {
            ++st.skipped_exclude;
            continue;
        }
        std::string_view ext = ext_of(fname);
        if (ext.empty()) {
            ++st.skipped_no_ext;
            continue;
        }
        std::string ext_lower = to_lower(ext);
        if (!opt.no_skip_lists) {
            if (kSkipExt.contains(ext_lower)) {
                ++st.skipped_builtin;
                continue;
            }
            std::string name_lower = to_lower(fname);
            if (kSkipNameLower.contains(name_lower)) {
                ++st.skipped_builtin;
                continue;
            }
        }
        if (opt.has_filter && !opt.ext_filter.contains(ext_lower)) {
            ++st.skipped_filter;
            continue;
        }
        std::string ext_dir;
        if (L.inc_ext) {
            ext_dir = opt.lowercase_ext ? ext_lower : std::string(ext);
        }
        fs::path parent;
        if (!L.inc_year) {
            if (ext_dir == last_ext && !last_parent.empty()) {
                parent = last_parent;
            } else {
                parent = base / ext_dir;
                last_ext = ext_dir;
                last_year.clear();
                last_month.clear();
                last_day.clear();
                last_parent = parent;
            }
        } else {
            YMD ymd = local_ymd(entry.last_write_time(e));
            if (e) {
                log_error(path, e);
                ++st.errors;
                continue;
            }
            std::string year = std::to_string(ymd.year);
            std::string month = L.inc_month ? two_digits(ymd.month) : std::string {
            };
            std::string day = L.inc_day ? two_digits(ymd.day) : std::string {
            };
            if (ext_dir == last_ext && year == last_year && month == last_month && day == last_day && !last_parent.empty()) {
                parent = last_parent;
            } else {
                parent = base;
                if (L.inc_ext)parent /= ext_dir;
                parent /= year;
                if (L.inc_month)parent /= month;
                if (L.inc_day)parent /= day;
                last_ext = ext_dir;
                last_year = year;
                last_month = month;
                last_day = day;
                last_parent = parent;
            }
        }
        fs::path dst = parent / fname;
        std::uintmax_t fsize = entry.file_size(e);
        if (e)fsize = 0;
        if (opt.dry_run) {
            if (opt.verbose)std::cout << "[dry-run] " << fname << "  ->  " << dst.string() << '\n';
            ++st.moved;
            st.moved_bytes += fsize;
            continue;
        }
        fs::create_directories(parent, e);
        if (e) {
            log_error(path, e);
            ++st.errors;
            continue;
        }
        std::error_code exists_ec;
        bool exists = fs::exists(dst, exists_ec);
        if (exists_ec) {
            log_error(path, exists_ec);
            ++st.errors;
            continue;
        }
        if (exists) {
            log_conflict(path, dst);
            ++st.conflicts;
            continue;
        }
        fs::rename(path, dst, e);
        if (e) {
            std::error_code copy_ec;
            fs::copy_file(path, dst, fs::copy_options::none, copy_ec);
            if (copy_ec) {
                log_error(path, e);
                ++st.errors;
                continue;
            }
            std::error_code rm_ec;
            fs::remove(path, rm_ec);
            if (rm_ec) {
                std::cerr << "[warn] copied but could not remove source: " << fname << ": " << rm_ec.message() << '\n';
            }
        }
        ++st.moved;
        st.moved_bytes += fsize;
        if (opt.verbose)std::cout << "[moved] " << fname << "  ->  " << dst.string() << '\n';
    }
    if (!opt.quiet) {
        std::cout << "\nSummary:\n" << "  mode      : " << layout_desc(L) << '\n' << "  moved     : " << st.moved << " (" << human_bytes(st.moved_bytes) << ")\n" << "  conflicts : " << st.conflicts << '\n' << "  errors    : " << st.errors << '\n' << "  skipped   : hidden=" << st.skipped_hidden << " no-ext=" << st.skipped_no_ext << " builtin=" << st.skipped_builtin << " exclude=" << st.skipped_exclude << " filter=" << st.skipped_filter << '\n';
    }
    return st.errors == 0 ? 0 : 1;
}
