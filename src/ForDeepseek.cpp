#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <vector>
#include <cstdlib>

// ---- Exclude these directories from the tree ----
static bool isExcluded(const std::filesystem::path& p) {
    std::string name = p.filename().string();
    return (name == ".git" || name == "build" || name == "exe" ||
            name == "output" || name == "input");
}

// Helper: get file size as string
static std::string getSizeStr(const std::filesystem::path& p) {
    if (!std::filesystem::is_regular_file(p)) return "";
    auto size = std::filesystem::file_size(p);
    return std::to_string(size) + " B";
}

// Recursively print the tree
static void printTree(const std::filesystem::path& path, int depth, std::ostream& out) {
    std::string indent(depth * 2, ' ');

    if (depth == 0) {
        out << path.filename().string() << "/" << std::endl;
    }

    std::vector<std::filesystem::directory_entry> entries;
    if (std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.path().filename() < b.path().filename(); });

    for (const auto& entry : entries) {
        const auto& p = entry.path();
        std::string name = p.filename().string();

        // Skip excluded directories, hidden files, and the report file itself
        if (name.empty() || name[0] == '.' || isExcluded(p)) continue;
        if (p.filename() == "ForDeepseek.txt") continue;

        if (std::filesystem::is_directory(p)) {
            out << indent << "├── " << name << "/" << std::endl;
            printTree(p, depth + 1, out);
        } else if (std::filesystem::is_regular_file(p)) {
            out << indent << "├── " << name << "  (" << getSizeStr(p) << ")" << std::endl;
        }
    }
}

// Dump all text files from src/ (including subfolders)
static void dumpSrcContent(const std::filesystem::path& srcDir, std::ostream& out) {
    if (!std::filesystem::exists(srcDir) || !std::filesystem::is_directory(srcDir)) {
        out << "\n⚠️  src/ directory not found – skipping content dump.\n";
        return;
    }

    out << "\n\n========== CONTENTS OF src/ DIRECTORY ==========\n";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(srcDir)) {
        const auto& p = entry.path();
        if (!std::filesystem::is_regular_file(p)) continue;

        // Skip binary extensions
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".tif" || ext == ".tiff" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".gif" || ext == ".bmp" || ext == ".ico") {
            continue;
        }

        std::string relPath = std::filesystem::relative(p, srcDir).string();
        out << "\n--- " << relPath << " ---\n";

        std::ifstream file(p);
        if (!file.is_open()) {
            out << "[Could not open file]\n";
            continue;
        }
        out << file.rdbuf();
        out << "\n";
    }
}

int main() {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories("output");

    std::ofstream out("output/ForDeepseek.txt");
    if (!out.is_open()) {
        std::cerr << "❌ Could not create output/ForDeepseek.txt" << std::endl;
        return 1;
    }

    std::time_t t = std::time(nullptr);
    out << "Generated: " << std::ctime(&t);
    out << "Project root: " << std::filesystem::current_path().string() << "\n\n";
    out << "========== DIRECTORY TREE ==========\n";

    printTree(std::filesystem::current_path(), 0, out);

    dumpSrcContent(std::filesystem::current_path() / "src", out);

    out << "\n========== END OF REPORT ==========\n";
    out.close();

    std::cout << "✅ Report written to output/ForDeepseek.txt" << std::endl;
    return 0;
}