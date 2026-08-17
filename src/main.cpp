/**
 * chelper-cli —— 我的世界基岩版命令辅助器（命令行版）
 * 基于 CHelper 开源内核 (GPLv3) https://github.com/Yancey2023/CHelper
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
};

// ---------- 帮助 ----------
static void printHelp() {
    std::cout
        << Color::BOLD << "chelper-cli" << Color::RESET
        << " —— 我的世界基岩版命令辅助器（基于 CHelper 开源内核，GPLv3）\n\n"
        << "用法:\n"
        << "  chelper                       进入交互式 REPL（输入命令实时分析）\n"
        << "  chelper --check \"<命令>\"      检查命令，显示语法错误与命令结构\n"
        << "  chelper --suggest \"<命令>\"    显示光标处的补全建议\n"
        << "  chelper --old2new \"<旧命令>\"  旧版语法转换为新版语法\n"
        << "  chelper --cpack <文件>        指定 cpack 资源文件（默认 release-vanilla）\n"
        << "  chelper -v / --version        显示版本\n\n"
        << "REPL 内命令:\n"
        << "  :q        退出\n"
        << "  :s <N>    应用第 N 条补全建议\n"
        << "  :h        显示本帮助\n";
}

static const char *VERSION = "0.1.0";

// ---------- 输出分析结果 ----------
static void printAnalysis(CHelper::CHelperCore &core) {
    // 结构
    auto structure = core.getStructure();
    if (!structure.empty()) {
        std::cout << Color::CYAN << "结构: " << Color::RESET << u16to8(structure) << "\n";
    }
    // 参数提示
    auto hint = core.getParamHint();
    if (!hint.empty()) {
        std::cout << Color::BLUE << "提示: " << Color::RESET << u16to8(hint) << "\n";
    }
    // 错误
    auto errors = core.getErrorReasons();
    for (const auto &e : errors) {
        std::cout << Color::RED << "错误: " << Color::RESET << u16to8(e->errorReason)
                  << Color::DIM << " [位置 " << e->start << "-" << e->end << "]" << Color::RESET << "\n";
    }
    // 建议
    auto *sugs = core.getSuggestions();
    if (sugs && !sugs->empty()) {
        std::cout << Color::GREEN << "建议: " << Color::RESET;
        int i = 1;
        for (const auto &s : *sugs) {
            std::string desc;
            if (s.content->description.has_value() && !s.content->description->empty()) {
                desc = Color::DIM + " (" + u16to8(s.content->description.value()) + ")" + Color::RESET;
            }
            std::cout << "[" << i << "]" << u16to8(s.content->name) << desc << "  ";
            ++i;
        }
        std::cout << "\n";
    }
}

// ---------- REPL 交互模式 ----------
static int repl(std::unique_ptr<CHelper::CHelperCore> core) {
    std::cout << Color::BOLD << "基岩版命令辅助器 " << Color::RESET << VERSION
              << "（输入 :h 查看帮助，:q 退出）\n";
    std::string line;
    std::u16string current;
    while (true) {
        std::cout << Color::GREEN << "mc> " << Color::RESET;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == ":q") break;
        if (line == ":h") { printHelp(); continue; }
        if (line.rfind(":s ", 0) == 0) {
            int n = 0;
            try { n = std::stoi(line.substr(3)); } catch (...) {}
            if (n <= 0) { std::cout << "用法: :s <N>\n"; continue; }
            auto result = core->onSuggestionClick(static_cast<size_t>(n - 1));
            if (result.has_value()) {
                current = result->first;
                std::cout << Color::GREEN << "已应用: " << Color::RESET << u16to8(current) << "\n";
                core->onTextChanged(current, result->second);
                printAnalysis(*core);
            } else {
                std::cout << Color::RED << "建议序号无效\n" << Color::RESET;
            }
            continue;
        }
        // 普通输入：当作命令分析
        current = u8to16(line);
        core->onTextChanged(current, current.size());
        printAnalysis(*core);
    }
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
        printAnalysis(*core);
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
