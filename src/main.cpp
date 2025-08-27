#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Task {
    int id{};
    bool done{false};
    char priority{'M'};                // H, M, L
    std::optional<std::string> due;    // YYYY-MM-DD
    std::string title;                 // no newlines; '|' not allowed
};

// util
static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\n\r");
    return s.substr(b, e - b + 1);
}

// validation of YYYY-MM-DD digits
static inline bool is_valid_date(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (size_t i = 0; i < d.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(d[i]))) return false;
    }
    return true;
}

static inline int prio_weight(char p) {
    switch (p) {
        case 'H': return 0;
        case 'M': return 1;
        case 'L': return 2;
        default:  return 1;
    }
}

// storage
static const char SEP = '|';

static fs::path data_path() {
    return fs::current_path() / "tasks.tsv";
}

static std::string encode_field(const std::string& s) {
    std::string out = s;
    for (char& c : out) if (c == SEP) c = '/';
    out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
    return out;
}

static std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    std::istringstream iss(line);
    while (std::getline(iss, cur, delim)) parts.push_back(cur);
    return parts;
}

static std::vector<Task> load_tasks() {
    std::vector<Task> v;
    std::ifstream in(data_path());
    if (!in) return v;
    std::string line;
    while (std::getline(in, line)) {
        if (trim(line).empty()) continue;
        auto parts = split(line, SEP);
        if (parts.size() < 5) continue;
        Task t;
        t.id = std::stoi(parts[0]);
        t.done = (parts[1] == "1");
        t.priority = parts[2].empty() ? 'M' : parts[2][0];
        if (parts[3] == "-") t.due.reset(); else t.due = parts[3];
        t.title = parts[4];
        v.push_back(std::move(t));
    }
    return v;
}

static void save_tasks(const std::vector<Task>& v) {
    std::ofstream out(data_path(), std::ios::trunc);
    for (const auto& t : v) {
        out << t.id << SEP
            << (t.done ? '1' : '0') << SEP
            << t.priority << SEP
            << (t.due ? *t.due : std::string("-")) << SEP
            << encode_field(t.title) << '\n';
    }
}

static int next_id(const std::vector<Task>& v) {
    int m = 0;
    for (const auto& t : v) m = std::max(m, t.id);
    return m + 1;
}

// printing
static void print_task(const Task& t) {
    std::cout << std::setw(3) << t.id << "  "
              << (t.done ? "[x]" : "[ ]") << "  "
              << t.priority << "  "
              << (t.due ? *t.due : std::string("--")) << "  "
              << t.title << "\n";
}

// list filtering
enum class ListFilter { All, Pending, Done };

static void list_cmd(ListFilter filter, const std::string& sort_key) {
    auto v = load_tasks();
    std::stable_sort(v.begin(), v.end(), [&](const Task& a, const Task& b){
        if (a.done != b.done) return !a.done; 
        if (sort_key == "due") {
            std::string ad = a.due.value_or("9999-99-99");
            std::string bd = b.due.value_or("9999-99-99");
            if (ad != bd) return ad < bd;
        } else if (sort_key == "priority") {
            int aw = prio_weight(a.priority), bw = prio_weight(b.priority);
            if (aw != bw) return aw < bw;
        } else if (sort_key == "id") {
            if (a.id != b.id) return a.id < b.id;
        }

        std::string ad = a.due.value_or("9999-99-99");
        std::string bd = b.due.value_or("9999-99-99");
        if (ad != bd) return ad < bd;
        int aw = prio_weight(a.priority), bw = prio_weight(b.priority);
        if (aw != bw) return aw < bw;
        return a.id < b.id;
    });

    for (const auto& t : v) {
        if (filter == ListFilter::Pending && t.done) continue;
        if (filter == ListFilter::Done && !t.done) continue;
        print_task(t);
    }
}

static void help() {
    std::cout << "Task Tracker (tt)\n\n"
              << "Usage:\n"
              << "  tt add <title words...> [-p H|M|L] [-d YYYY-MM-DD]\n"
              << "  tt list [--all|--pending|--done] [--sort=due|priority|id]\n"
              << "  tt done <id>\n"
              << "  tt rm <id>\n"
              << "  tt clear [--done|--all]\n"
              << "  tt help\n\n"
              << "Notes:\n"
              << "  - Default 'tt list' now shows ALL tasks. Completed ones display as [x].\n"
              << "  - Use --pending to show only pending, or --done to show only completed.\n\n"
              << "Data file: tasks.tsv (in current directory)\n";
}

static int parse_int(const std::string& s) {
    try { return std::stoi(s); } catch (...) { return -1; }
}

// main
int main(int argc, char** argv) {
    if (argc < 2) { help(); return 0; }
    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "-h" || cmd == "--help") { help(); return 0; }

    if (cmd == "add") {
        if (argc < 3) { std::cerr << "Error: title required.\n"; return 1; }
        char pr = 'M';
        std::optional<std::string> due;
        std::vector<std::string> title_parts;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "-p" && i + 1 < argc) {
                pr = std::toupper(static_cast<unsigned char>(argv[++i][0]));
                if (pr != 'H' && pr != 'M' && pr != 'L') {
                    std::cerr << "Invalid priority. Use H/M/L.\n"; return 1;
                }
            } else if (a == "-d" && i + 1 < argc) {
                std::string d = argv[++i];
                if (!is_valid_date(d)) {
                    std::cerr << "Invalid date, expected YYYY-MM-DD.\n"; return 1;
                }
                due = d;
            } else if (!a.empty() && a[0] == '-') {
                std::cerr << "Unknown flag: " << a << "\n"; return 1;
            } else {
                title_parts.push_back(a);
            }
        }
        if (title_parts.empty()) { std::cerr << "Error: title required.\n"; return 1; }
        std::string title;
        for (size_t i = 0; i < title_parts.size(); ++i) {
            if (i) title += ' ';
            title += title_parts[i];
        }
        auto v = load_tasks();
        Task t; t.id = next_id(v); t.priority = pr; t.due = due; t.title = trim(title);
        v.push_back(std::move(t));
        save_tasks(v);
        std::cout << "Added task #" << v.back().id << "\n";
        return 0;
    }

    if (cmd == "list") {
        ListFilter filter = ListFilter::All;       
        // default sort
        std::string sort_key = "due";              
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--all")        { filter = ListFilter::All; }
            else if (a == "--pending"){ filter = ListFilter::Pending; }
            else if (a == "--done")  { filter = ListFilter::Done; }
            else if (a.rfind("--sort=", 0) == 0) sort_key = a.substr(7);
            else { std::cerr << "Unknown arg: " << a << "\n"; return 1; }
        }
        list_cmd(filter, sort_key);
        return 0;
    }

    if (cmd == "done") {
        if (argc < 3) { std::cerr << "Usage: tt done <id>\n"; return 1; }
        int id = parse_int(argv[2]); if (id < 0) { std::cerr << "Invalid id.\n"; return 1; }
        auto v = load_tasks(); bool found = false;
        for (auto& t : v) if (t.id == id) { t.done = true; found = true; break; }
        if (!found) { std::cerr << "Task not found.\n"; return 1; }
        save_tasks(v); std::cout << "Marked #" << id << " done.\n"; return 0;
    }

    if (cmd == "rm") {
        if (argc < 3) { std::cerr << "Usage: tt rm <id>\n"; return 1; }
        int id = parse_int(argv[2]); if (id < 0) { std::cerr << "Invalid id.\n"; return 1; }
        auto v = load_tasks();
        auto it = std::remove_if(v.begin(), v.end(), [&](const Task& t){ return t.id == id; });
        if (it == v.end()) { std::cerr << "Task not found.\n"; return 1; }
        v.erase(it, v.end()); save_tasks(v); std::cout << "Removed #" << id << ".\n"; return 0;
    }

    if (cmd == "clear") {
        bool clear_all = false;
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if      (a == "--all") clear_all = true;
            else if (a == "--done") { /* default behavior */ }
            else { std::cerr << "Unknown arg: " << a << "\n"; return 1; }
        }
        auto v = load_tasks();
        if (clear_all) v.clear();
        else v.erase(std::remove_if(v.begin(), v.end(), [](const Task& t){ return t.done; }), v.end());
        save_tasks(v);
        std::cout << (clear_all ? "Cleared all tasks." : "Cleared completed tasks.") << "\n";
        return 0;
    }

    std::cerr << "Unknown command. Try 'tt help'.\n";
    return 1;
}
