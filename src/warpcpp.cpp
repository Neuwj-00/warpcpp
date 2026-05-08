#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include <algorithm>
#include <string_view>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <future>
#include <format>
#include <ranges>
#include <charconv>
#include <system_error>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

using namespace std::string_view_literals;

constexpr std::string_view CF_ORANGE = "\033[38;2;243;128;32m";
constexpr std::string_view CF_YELLOW = "\033[38;2;250;173;63m";
constexpr std::string_view CF_BRIGHT = "\033[38;2;255;213;128m";
constexpr std::string_view GREEN     = "\033[1;32m";
constexpr std::string_view RED       = "\033[1;31m";
constexpr std::string_view CYAN      = "\033[1;36m";
constexpr std::string_view RESET     = "\033[0m";
constexpr std::string_view BOLD      = "\033[1m";

namespace Core {
    bool is_safe_input(std::string_view input) {
        if (input.empty()) return false;
        return std::ranges::all_of(input, [](unsigned char c) {
            return std::isalnum(c) || c == '.' || c == '-' || c == ':';
        });
    }

    std::string run_warp_cli(const std::vector<std::string>& args) {
        int pipefd[2];
        if (pipe(pipefd) == -1) return "ERROR: Pipe creation failed.";

        pid_t pid = fork();
        if (pid == -1) return "ERROR: Fork failed.";

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            std::vector<char*> c_args;
            c_args.push_back(const_cast<char*>("warp-cli"));
            for (const auto& arg : args) c_args.push_back(const_cast<char*>(arg.c_str()));
            c_args.push_back(nullptr);

            execvp("warp-cli", c_args.data());
            std::cerr << "execvp error: warp-cli not found or permission denied.\n";
            exit(EXIT_FAILURE);
        } else {
            close(pipefd[1]);
            std::array<char, 512> buffer;
            std::string result;
            ssize_t bytes_read;
            
            while ((bytes_read = read(pipefd[0], buffer.data(), buffer.size())) > 0) {
                result.append(buffer.data(), bytes_read);
            }
            close(pipefd[0]);
            waitpid(pid, nullptr, 0); 
            return result;
        }
    }

    bool is_warp_svc_running() {
        try {
            for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
                if (entry.is_directory()) {
                    std::string dirname = entry.path().filename().string();
                    if (!dirname.empty() && std::ranges::all_of(dirname, [](unsigned char c){ return std::isdigit(c); })) {
                        std::ifstream comm_file(entry.path() / "comm");
                        std::string comm_name;
                        if (comm_file >> comm_name && comm_name == "warp-svc") return true;
                    }
                }
            }
        } catch (...) { }
        return false;
    }

    bool is_registered() {
        std::string out = run_warp_cli({"registration", "show"});
        return out.contains("Account type") || out.contains("Device ID");
    }

    void get_status_info(std::string& status_text, std::string& status_color) {
        std::string output = run_warp_cli({"status"});
        std::string lower_out = output;
        
        std::ranges::transform(lower_out, lower_out.begin(), [](unsigned char c){ return std::tolower(c); });

        if (lower_out.contains("connected") && !lower_out.contains("disconnected")) {
            status_text = "CONNECTED";
            status_color = std::string(GREEN);
        } else if (lower_out.contains("disconnected")) {
            status_text = "DISCONNECTED";
            status_color = std::string(CF_YELLOW);
        } else if (lower_out.contains("connecting")) {
            status_text = "CONNECTING...";
            status_color = std::string(CYAN);
        } else {
            status_text = "OFFLINE / ERROR";
            status_color = std::string(RED);
        }
    }
}

int get_terminal_width() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) return 80;
    return w.ws_col;
}

std::string get_separator() {
    return std::string(get_terminal_width(), '-');
}

void clear_screen() {
    std::cout << "\033[2J\033[H";
    std::cout.flush();
}

void pause_screen() {
    std::cout << "\n  " << CF_YELLOW << "Press Enter to continue..." << RESET;
    std::string dummy;
    std::getline(std::cin, dummy);
}

void print_output(const std::string& output) {
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            std::cout << "  " << line << "\n";
        }
    }
}

void print_header() {
    std::cout << "\n";
    std::cout << "  " << BOLD << CF_ORANGE << "☁️  CLOUD"
              << CF_YELLOW << "FLARE "
              << CF_BRIGHT << "WARP+ " << RESET << "v3.0\n";
    std::cout << "  " << CF_YELLOW << "made by Neuwj - neuwj@bk.ru\n" << RESET;
    std::cout << CF_ORANGE << get_separator() << "\n" << RESET;
}

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int parse_choice(std::string_view choice) {
    auto start = choice.find_first_not_of(" \t");
    auto end = choice.find_last_not_of(" \t");
    if (start == std::string_view::npos) return -1;
    
    std::string_view trimmed = choice.substr(start, end - start + 1);
    int c = -1;
    auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), c);
    
    if (ec != std::errc() || ptr != trimmed.data() + trimmed.size()) return -1;
    return c;
}

void dns_filters_menu() {
    std::string choice;
    while (true) {
        clear_screen();
        print_header();

        std::cout << "  " << BOLD << "🛡️  DNS Families & Filters" << RESET << "\n\n";
        std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " Off (No Filtering)\n";
        std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " Malware Only\n";
        std::cout << "  " << CF_BRIGHT << "[3]" << RESET << " Full (Malware + Adult Content)\n";
        std::cout << "  " << CF_BRIGHT << "[4]" << RESET << " Back to Main Menu\n";
        std::cout << "\n  Select (1-4): ";

        if (!std::getline(std::cin, choice) || choice == "4") break;

        int c = parse_choice(choice);
        constexpr std::array families = {""sv, "off"sv, "malware"sv, "full"sv};
        
        if (c >= 1 && c <= 3) {
            std::cout << "\n";
            print_output(Core::run_warp_cli({"dns", "families", std::string(families[c])}));
            sleep_ms(1500);
        }
    }
}

void advanced_menu() {
    std::string choice;
    while (true) {
        clear_screen();
        print_header();

        std::cout << "  " << BOLD << "🔧  Advanced & Diagnostics" << RESET << "\n\n";
        std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " Display Current Settings\n";
        std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " View Network Information\n";
        std::cout << "  " << CF_BRIGHT << "[3]" << RESET << " Connection Statistics (Enhanced)\n";
        std::cout << "  " << CF_BRIGHT << "[4]" << RESET << " Device Posture Information\n";
        std::cout << "  " << CF_BRIGHT << "[5]" << RESET << " Back to Main Menu\n";
        std::cout << "\n  Select (1-5): ";

        if (!std::getline(std::cin, choice) || choice == "5") break;

        std::cout << "\n";
        int c = parse_choice(choice);
        
        if (c == 1) { print_output(Core::run_warp_cli({"settings", "list"})); pause_screen(); }
        else if (c == 2) { print_output(Core::run_warp_cli({"debug", "network"})); pause_screen(); }
        else if (c == 3) { print_output(Core::run_warp_cli({"stats", "enhanced"})); pause_screen(); }
        else if (c == 4) { print_output(Core::run_warp_cli({"debug", "posture"})); pause_screen(); }
    }
}

void setup_menu() {
    std::string choice;
    while (true) {
        clear_screen();
        print_header();

        std::cout << "  " << BOLD << "⚙️  Setup & Settings" << RESET << "\n\n";
        std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " Register New Device (First Time Setup)\n";
        std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " Apply License Key\n";
        std::cout << "  " << CF_BRIGHT << "[3]" << RESET << " View Registration Info\n";
        std::cout << "  " << CF_BRIGHT << "[4]" << RESET << " Change Mode\n";
        std::cout << "  " << CF_BRIGHT << "[5]" << RESET << " Change Protocol (WireGuard / MASQUE)\n";
        std::cout << "  " << CF_BRIGHT << "[6]" << RESET << " Back to Main Menu\n";
        std::cout << "\n  Select (1-6): ";

        if (!std::getline(std::cin, choice) || choice == "6") break;

        int c = parse_choice(choice);

        if (c == 1) {
            std::cout << "\n  " << BOLD << CF_ORANGE << ">> Registering device... (Accepting TOS)" << RESET << "\n";
            print_output(Core::run_warp_cli({"--accept-tos", "registration", "new"}));
            sleep_ms(2000);
        }
        else if (c == 2) {
            std::string key;
            std::cout << "\n  " << BOLD << "License Key: " << RESET;
            std::getline(std::cin, key);
            if (Core::is_safe_input(key)) {
                std::cout << "\n  " << BOLD << CF_ORANGE << ">> Applying License Key..." << RESET << "\n";
                print_output(Core::run_warp_cli({"registration", "license", key}));
            } else {
                std::cout << "\n  " << RED << ">> Invalid characters detected. Cancelled." << RESET << "\n";
            }
            sleep_ms(2000);
        }
        else if (c == 3) {
            std::cout << "\n";
            print_output(Core::run_warp_cli({"registration", "show"}));
            pause_screen();
        }
        else if (c == 4) {
            std::cout << "\n  " << BOLD << "Select Mode:" << RESET << "\n";
            std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " Standard Warp+\n";
            std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " DNS Only (1.1.1.1)\n";
            std::cout << "  " << CF_BRIGHT << "[3]" << RESET << " Warp+ & DNS\n";
            std::cout << "  " << CF_BRIGHT << "[4]" << RESET << " Proxy (Local SOCKS5)\n";
            std::cout << "  Choice: ";
            
            std::string mod; std::getline(std::cin, mod);
            int m = parse_choice(mod);
            constexpr std::array modes = {""sv, "warp"sv, "doh"sv, "warp+doh"sv, "proxy"sv};
            if (m >= 1 && m <= 4) {
                std::cout << "\n";
                print_output(Core::run_warp_cli({"mode", std::string(modes[m])}));
            }
            sleep_ms(1500);
        }
        else if (c == 5) {
            std::cout << "\n  " << BOLD << "Select Protocol:" << RESET << "\n";
            std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " WireGuard\n";
            std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " MASQUE (Recommended)\n";
            std::cout << "  Choice: ";

            std::string proto; std::getline(std::cin, proto);
            int p = parse_choice(proto);
            constexpr std::array protos = {""sv, "WireGuard"sv, "MASQUE"sv};
            if (p >= 1 && p <= 2) {
                std::cout << "\n";
                print_output(Core::run_warp_cli({"tunnel", "protocol", "set", std::string(protos[p])}));
            }
            sleep_ms(1500);
        }
    }
}

int main() {
    if (!Core::is_warp_svc_running()) {
        clear_screen();
        print_header();
        std::cout << "\n  " << RED << BOLD << "Critical Error: 'warp-svc' daemon is not running!" << RESET << "\n";
        std::cout << "  Please start the background service (e.g. 'sudo systemctl start warp-svc').\n";
        pause_screen();
    }

    std::string choice;
    while (true) {
        std::string status_text, status_color;
        Core::get_status_info(status_text, status_color);

        clear_screen();
        print_header();

        std::cout << "  " << BOLD << "Network Info:" << RESET << "\n";
        std::cout << "  Current Status: [" << status_color << status_text << RESET << "]\n\n";

        std::cout << "  What would you like to do?\n\n";
        std::cout << "  " << CF_BRIGHT << "[1]" << RESET << " Connect\n";
        std::cout << "  " << CF_BRIGHT << "[2]" << RESET << " Disconnect\n";
        std::cout << "  " << CF_BRIGHT << "[3]" << RESET << " Setup & Settings\n";
        std::cout << "  " << CF_BRIGHT << "[4]" << RESET << " DNS Families & Filters\n";
        std::cout << "  " << CF_BRIGHT << "[5]" << RESET << " Advanced & Diagnostics\n";
        std::cout << "  " << CF_BRIGHT << "[6]" << RESET << " Refresh Status\n";
        std::cout << "  " << CF_BRIGHT << "[7]" << RESET << " Exit\n";
        std::cout << "\n  Select (1-7): ";

        if (!std::getline(std::cin, choice)) break;

        int main_c = parse_choice(choice);

        if (main_c == 7 || choice == "7") {
            std::cout << "\n  " << BOLD << CF_ORANGE << "Stay safe, Neuwj! 👋\n" << RESET;
            sleep_ms(1000);
            clear_screen();
            break;
        }
        else if (main_c == 1) {
            if (!Core::is_registered()) {
                std::cout << "\n  " << CF_YELLOW << ">> It looks like you are using WARP for the first time." << RESET << "\n";
                std::cout << "  " << CF_ORANGE << ">> You must accept Cloudflare's Terms of Service (TOS) to register the device." << RESET << "\n";
                std::cout << "  " << "Do you accept and want to register? (Y/n): ";
                
                std::string ans;
                std::getline(std::cin, ans);
                if (ans.empty() || ans == "Y" || ans == "y") {
                    std::cout << "  " << CF_ORANGE << ">> Registering device..." << RESET << "\n";
                    Core::run_warp_cli({"--accept-tos", "registration", "new"});
                    sleep_ms(1500);
                } else {
                    std::cout << "  " << RED << ">> Registration cancelled. Connection aborted." << RESET << "\n";
                    sleep_ms(2000);
                    continue; 
                }
            }

            std::cout << "\n  " << BOLD << CF_ORANGE << ">> Initiating Connection... " << RESET;
            std::cout.flush();

            auto future = std::async(std::launch::async, []() { return Core::run_warp_cli({"connect"}); });
            const char spinner[] = {'|', '/', '-', '\\'};
            int i = 0;

            while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
                std::cout << "\b" << spinner[i++ % 4] << std::flush;
            }

            std::cout << "\b" << GREEN << "Done!" << RESET << "\n";
            print_output(future.get());
            sleep_ms(2000);
        }
        else if (main_c == 2) {
            std::cout << "\n  " << BOLD << CF_YELLOW << ">> Breaking Connection..." << RESET << "\n";
            print_output(Core::run_warp_cli({"disconnect"}));
            sleep_ms(1500);
        }
        else if (main_c == 3) setup_menu();
        else if (main_c == 4) dns_filters_menu();
        else if (main_c == 5) advanced_menu();
        else if (main_c == 6) continue;
    }

    return 0;
}
