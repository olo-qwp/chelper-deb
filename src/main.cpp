/**
 * chelper-cli —— 我的世界基岩版命令辅助器（命令行版）
 * 基于 CHelper 开源内核 (GPLv3) https://github.com/Yancey2023/CHelper
 * 交互模式带悬浮补全窗（终端浮层）
 */
#include <chelper/CHelperCore.h>
#include <chelper/old2new/Old2New.h>
#include <pch.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace fs = std::filesystem;

// ---------- UTF-8 / UTF-16 转换 ----------
static std::string u16to8(const std::u16string &s) {
    std::string out;
    utf8::utf16to8(s.begin(), s.end(), std::back_inserter(out));
    return out;
}
static std::u16string u8to16(const std::string &s) {
    std::u16string out;
    utf8::utf8to16(s.begin(), s.end(), std::back_inserter(out));
    return out;
}

// ---------- ANSI 颜色 ----------
struct Color {
    static constexpr const char *RESET = "\033[0m";
    static constexpr const char *RED = "\033[31m";
    static constexpr const char *GREEN = "\033[32m";
    static constexpr const char *YELLOW = "\033[33m";
    static constexpr const char *BLUE = "\033[34m";
    static constexpr const char *MAGENTA = "\033[35m";
    static constexpr const char *CYAN = "\033[36m";
    static constexpr const char *BOLD = "\033[1m";
    static constexpr const char *DIM = "\033[2m";
    static constexpr const char *REVERSE = "\033[7m";
};

// ---------- 帮助 ----------
static void printHelp() {
    std::cout
        << Color::BOLD << "chelper-cli" << Color::RESET
        << " —— 我的世界基岩版命令辅助器（基于 CHelper 开源内核，GPLv3）\n\n"
        << "用法:\n"
        << "  chelper                       进入交互模式（悬浮补全窗）\n"
        << "  chelper --check \"<命令>\"      检查命令，显示语法错误与命令结构\n"
        << "  chelper --suggest \"<命令>\"    显示光标处的补全建议\n"
        << "  chelper --old2new \"<旧命令>\"  旧版语法转换为新版语法\n"
        << "  chelper --cpack <文件>        指定 cpack 资源文件（默认 release-vanilla）\n"
        << "  chelper -v / --version        显示版本\n\n"
        << "交互模式按键:\n"
        << "  Tab / Enter    应用补全建议      ↑/↓     切换建议\n"
        << "  ←/→            移动光标          Backspace 删除\n"
        << "  Esc / Ctrl-C   退出\n";
}

static const char *VERSION = "0.2.0";

// ---------- 终端控制 ----------
struct Term {
    termios orig{};
    bool raw = false;

    void enable() {
        if (tcgetattr(STDIN_FILENO, &orig) != 0) return;
        termios r = orig;
        r.c_lflag &= ~(ICANON | ECHO | ISIG);
        r.c_cc[VMIN] = 1;
        r.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &r);
        raw = true;
    }
    void disable() {
        if (raw) {
            tcsetattr(STDIN_FILENO, TCSANOW, &orig);
            raw = false;
        }
    }
};

static int termWidth() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 80;
}

static std::string truncate(const std::string &s, size_t max) {
    if (s.size() <= max) return s;
    if (max <= 3) return s.substr(0, max);
    return s.substr(0, max - 3) + "...";
}

// ---------- 悬浮窗渲染 ----------
// data: 建议列表 (名称+描述)，highlight: 当前高亮索引
static void renderFloating(CHelper::CHelperCore &core,
                           const std::u16string &input, size_t cursor,
                           const std::vector<CHelper::AutoSuggestion::Suggestion> &sugs,
                           size_t highlight) {
    int W = termWidth();
    std::string line = u16to8(input);
    // 光标处插入 ▌
    std::string head = u16to8(input.substr(0, cursor));
    std::string tail = u16to8(input.substr(cursor));
    std::string shown = head + "▌" + tail;
    shown = truncate(shown, (size_t)W - 6);

    std::cout << "\033[2J\033[H";
    std::cout << Color::GREEN << "mc> " << Color::RESET << shown << "\n";

    // 悬浮补全窗
    if (!sugs.empty()) {
        const size_t MAX_SHOW = 10;
        size_t n = std::min(sugs.size(), MAX_SHOW);
        std::cout << Color::MAGENTA << "┌─ 补全建议 ──────────────────────────────" << Color::RESET << "\n";
        for (size_t i = 0; i < n; ++i) {
            const auto &s = sugs[i];
            std::string name = u16to8(s.content->name);
            std::string desc;
            if (s.content->description.has_value() && !s.content->description->empty()) {
                desc = u16to8(s.content->description.value());
            }
            std::string entry = "  " + name;
            if (!desc.empty()) entry += std::string(Color::DIM) + "  " + desc + Color::RESET;
            entry = truncate(entry, (size_t)W - 2);
            if (i == highlight) {
                std::cout << Color::REVERSE << entry << Color::RESET << "\n";
            } else {
                std::cout << entry << "\n";
            }
        }
        std::cout << Color::MAGENTA << "└─────────────────────────────────────────" << Color::RESET
                  << Color::DIM << "  Tab 应用  ↑↓ 选择  Esc 退出" << Color::RESET << "\n";
    }

    // 结构 / 提示 / 错误
    std::string structure = u16to8(core.getStructure());
    if (!structure.empty()) {
        std::cout << Color::CYAN << "结构: " << Color::RESET << truncate(structure, (size_t)W - 6) << "\n";
    }
    std::string hint = u16to8(core.getParamHint());
    if (!hint.empty()) {
        std::cout << Color::BLUE << "提示: " << Color::RESET << truncate(hint, (size_t)W - 6) << "\n";
    }
    auto errors = core.getErrorReasons();
    for (const auto &e : errors) {
        std::cout << Color::RED << "错误: " << Color::RESET << truncate(u16to8(e->errorReason), (size_t)W - 6)
                  << Color::DIM << " [位置 " << e->start << "-" << e->end << "]" << Color::RESET << "\n";
    }
    std::cout.flush();
}

// 把建议列表转成 vector（getSuggestions 返回 vector<Suggestion>*，拷贝一份）
static std::vector<CHelper::AutoSuggestion::Suggestion>
collectSuggestions(CHelper::CHelperCore &core) {
    std::vector<CHelper::AutoSuggestion::Suggestion> v;
    auto *sugs = core.getSuggestions();
    if (sugs) v = *sugs;
    return v;
}

// ---------- 交互模式（悬浮窗） ----------
static int repl(std::unique_ptr<CHelper::CHelperCore> core) {
    Term term;
    term.enable();

    std::u16string input;
    size_t cursor = 0;      // u16 字符索引
    size_t highlight = 0;
    bool quit = false;

    auto analyze = [&]() {
        core->onTextChanged(input, cursor);
        auto sugs = collectSuggestions(*core);
        if (highlight >= sugs.size()) highlight = 0;
        renderFloating(*core, input, cursor, sugs, highlight);
        return sugs;
    };

    // 初始渲染（空输入）
    analyze();

    while (!quit) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;

        if (c == 0x03) {
            // Ctrl-C：退出
            quit = true;
            break;
        }
        if (c == 0x1b) {
            // Esc 或方向键序列
            char seq[2];
            ssize_t n1 = read(STDIN_FILENO, &seq[0], 1);
            if (n1 != 1 || seq[0] != '[') { quit = true; break; }  // 单独 Esc
            if (read(STDIN_FILENO, &seq[1], 1) != 1) { quit = true; break; }
            switch (seq[1]) {
                case 'A': { // ↑
                    auto sugs = collectSuggestions(*core);
                    if (!sugs.empty()) {
                        if (highlight == 0) highlight = sugs.size() - 1;
                        else --highlight;
                        renderFloating(*core, input, cursor, sugs, highlight);
                    }
                    break;
                }
                case 'B': { // ↓
                    auto sugs = collectSuggestions(*core);
                    if (!sugs.empty()) {
                        highlight = (highlight + 1) % sugs.size();
                        renderFloating(*core, input, cursor, sugs, highlight);
                    }
                    break;
                }
                case 'C': { // →
                    if (cursor < input.size()) {
                        ++cursor;
                        analyze();
                    }
                    break;
                }
                case 'D': { // ←
                    if (cursor > 0) {
                        --cursor;
                        analyze();
                    }
                    break;
                }
                default: break;
            }
            continue;
        }
        if (c == '\r' || c == '\n') {
            // Enter：应用高亮建议
            auto sugs = collectSuggestions(*core);
            if (!sugs.empty() && highlight < sugs.size()) {
                auto result = core->onSuggestionClick(highlight);
                if (result.has_value()) {
                    input = result->first;
                    cursor = result->second;
                    highlight = 0;
                    analyze();
                    continue;
                }
            }
            continue;
        }
        if (c == '\t') {
            // Tab：应用第一条建议
            auto sugs = collectSuggestions(*core);
            if (!sugs.empty()) {
                auto result = core->onSuggestionClick(0);
                if (result.has_value()) {
                    input = result->first;
                    cursor = result->second;
                    highlight = 0;
                    analyze();
                }
            }
            continue;
        }
        if (c == 0x7f) {
            // Backspace
            if (cursor > 0) {
                input.erase(cursor - 1, 1);
                --cursor;
                analyze();
            }
            continue;
        }
        if (c == 0x1b) { // 不应到达（上面已处理），保留分支
            continue;
        }
        // 可打印字符（含 UTF-8 起始字节）
        if ((unsigned char)c >= 0x20) {
            std::string utf8ch;
            utf8ch += c;
            if ((unsigned char)c >= 0x80) {
                // 读取完整 UTF-8 序列
                int extra = ((unsigned char)c >= 0xF0) ? 3 : ((unsigned char)c >= 0xE0) ? 2 : 1;
                for (int i = 0; i < extra; ++i) {
                    char cc;
                    if (read(STDIN_FILENO, &cc, 1) != 1) break;
                    utf8ch += cc;
                }
            }
            try {
                std::u16string ch = u8to16(utf8ch);
                input.insert(cursor, ch);
                cursor += ch.size();
                analyze();
            } catch (...) {
                // 无效 UTF-8，忽略
            }
        }
    }

    term.disable();
    std::cout << "\033[2J\033[H";
    return 0;
}

// ---------- main ----------
int main(int argc, char **argv) {
    std::string cpackPath = RESOURCE_DIR "/release-vanilla-1.21.132.1.cpack";
    std::string mode = "repl";
    std::string arg;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-v" || a == "--version") {
            std::cout << "chelper-cli " << VERSION << "\n";
            return 0;
        }
        if (a == "-h" || a == "--help") { printHelp(); return 0; }
        if (a == "--cpack") {
            if (i + 1 < argc) { cpackPath = argv[++i]; }
            continue;
        }
        if (a == "--check" || a == "--suggest" || a == "--old2new") {
            mode = a.substr(2);
            if (i + 1 < argc) arg = argv[++i];
            continue;
        }
        std::cerr << "未知参数: " << a << "（-h 查看帮助）\n";
        return 1;
    }

    // 加载 cpack 资源
    if (!fs::exists(cpackPath)) {
        std::cerr << Color::RED << "找不到 cpack 资源: " << cpackPath
                  << "\n请用 --cpack 指定资源文件。\n" << Color::RESET;
        return 1;
    }
    auto core = std::unique_ptr<CHelper::CHelperCore>(
        CHelper::CHelperCore::createByDirectory(cpackPath));
    if (!core) {
        std::cerr << Color::RED << "cpack 资源加载失败\n" << Color::RESET;
        return 1;
    }

    if (mode == "repl") return repl(std::move(core));

    // 一次性模式
    auto u16 = u8to16(arg);
    if (mode == "old2new") {
        // 加载方块修复数据
        std::string jsonPath = RESOURCE_DIR "/blockFixData.json";
        if (!fs::exists(jsonPath)) {
            std::cerr << Color::RED << "找不到 " << jsonPath << "\n" << Color::RESET;
            return 1;
        }
        std::ifstream ifs(jsonPath);
        if (!ifs) { std::cerr << "无法打开 blockFixData.json\n"; return 1; }
        std::stringstream ss;
        ss << ifs.rdbuf();
        rapidjson::Document doc;
        doc.Parse(ss.str().c_str());
        auto fixData = CHelper::Old2New::blockFixDataFromJson(doc);
        auto result = CHelper::CHelperCore::old2new(fixData, u16);
        std::cout << u16to8(result) << "\n";
        return 0;
    }

    core->onTextChanged(u16, u16.size());
    if (mode == "check") {
        std::string structure = u16to8(core->getStructure());
        if (!structure.empty()) {
            std::cout << Color::CYAN << "结构: " << Color::RESET << structure << "\n";
        }
        std::string hint = u16to8(core->getParamHint());
        if (!hint.empty()) {
            std::cout << Color::BLUE << "提示: " << Color::RESET << hint << "\n";
        }
        auto errors = core->getErrorReasons();
        for (const auto &e : errors) {
            std::cout << Color::RED << "错误: " << Color::RESET << u16to8(e->errorReason)
                      << Color::DIM << " [位置 " << e->start << "-" << e->end << "]" << Color::RESET << "\n";
        }
        auto *sugs = core->getSuggestions();
        if (sugs && !sugs->empty()) {
            std::cout << Color::GREEN << "建议: " << Color::RESET;
            for (const auto &s : *sugs) {
                std::cout << u16to8(s.content->name) << "  ";
            }
            std::cout << "\n";
        }
    } else if (mode == "suggest") {
        auto *sugs = core->getSuggestions();
        if (sugs) {
            for (const auto &s : *sugs) {
                std::cout << u16to8(s.content->name);
                if (s.content->description.has_value() && !s.content->description->empty()) {
                    std::cout << "\t" << u16to8(s.content->description.value());
                }
                std::cout << "\n";
            }
        }
    }
    return 0;
}
