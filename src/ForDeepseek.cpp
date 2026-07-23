#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace fs = std::filesystem;

// Helper to get file size as string (bytes)
static std::string getSizeStr(const fs::path& p) {
    if (!fs::is_regular_file(p)) return "";
    auto size = fs::file_size(p);
    return std::to_string(size) + " B";
}

// Print one level of the tree recursively
static void printTree(const fs::path& path, int depth, std::ostream& out) {
    std::string indent(depth * 2, ' ');  // two spaces per level

    if (depth == 0) {
        out << path.filename().string() << "/" << std::endl;
    }

    // Collect directory entries, sort by name
    std::vector<fs::directory_entry> entries;
    if (fs::is_directory(path)) {
        for (const auto& entry : fs::directory_iterator(path)) {
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.path().filename() < b.path().filename(); });

    for (const auto& entry : entries) {
        const auto& p = entry.path();
        std::string name = p.filename().string();

        // Skip hidden files and the output file itself (to avoid recursion)
        if (name[0] == '.' || p.filename() == "ForDeepseek.txt") continue;

        if (fs::is_directory(p)) {
            out << indent << "├── " << name << "/" << std::endl;
            printTree(p, depth + 1, out);
        } else if (fs::is_regular_file(p)) {
            out << indent << "├── " << name << "  (" << getSizeStr(p) << ")" << std::endl;
        }
    }
}

// Dump content of all files inside src/ (and its subdirectories)
static void dumpSrcContent(const fs::path& srcDir, std::ostream& out) {
    if (!fs::exists(srcDir) || !fs::is_directory(srcDir)) {
        out << "\n⚠️  src/ directory not found – skipping content dump.\n";
        return;
    }

    out << "\n\n========== CONTENTS OF src/ DIRECTORY ==========\n";

    // Recursively iterate over src/
    for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
        const auto& p = entry.path();
        if (!fs::is_regular_file(p)) continue;

        // Skip large binary files? For safety, we'll only read text files (no .tif, .png, etc.)
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".tif" || ext == ".tiff" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".gif" || ext == ".bmp" || ext == ".ico") {
            continue;
        }

        // Write a header with relative path
        std::string relPath = fs::relative(p, srcDir).string();
        out << "\n--- " << relPath << " ---\n";

        // Read and dump the file content
        std::ifstream file(p);
        if (!file.is_open()) {
            out << "[Could not open file]\n";
            continue;
        }
        out << file.rdbuf();
        out << "\n";  // ensure trailing newline
    }
}

int main() {
    std::ofstream out("ForDeepseek.txt");
    if (!out.is_open()) {
        std::cerr << "❌ Could not create ForDeepseek.txt" << std::endl;
        return 1;
    }

    // Print timestamp and project root
    std::time_t t = std::time(nullptr);
    out << "Generated: " << std::ctime(&t);
    out << "Project root: " << fs::current_path().string() << "\n\n";
    out << "========== DIRECTORY TREE ==========\n";

    // Print tree from current directory
    printTree(fs::current_path(), 0, out);

    // Dump src/ content
    dumpSrcContent(fs::current_path() / "src", out);

    out << "\n========== END OF REPORT ==========\n";
    out.close();

    std::cout << "✅ Report written to ForDeepseek.txt" << std::endl;
    return 0;
}