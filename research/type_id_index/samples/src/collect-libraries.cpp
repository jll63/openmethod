#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/dll/shared_library.hpp>
#include <elf.h>

namespace fs = boost::filesystem;
namespace json = boost::json;
namespace dll = boost::dll;

struct library_info {
    std::string path;
    size_t typeinfo_count;
};

// Check if a file is a shared library and contains type_info symbols
bool contains_type_info(const std::string& path, size_t& count) {
    count = 0;

    // Try to open the library with Boost.DLL
    boost::system::error_code ec;
    dll::shared_library lib(path, dll::load_mode::rtld_lazy, ec);

    if (ec) {
        return false;
    }

    // Read the ELF file to count type_info symbols
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read ELF header
    Elf64_Ehdr ehdr;
    file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

    if (file.gcount() != sizeof(ehdr) ||
        std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        return false;
    }

    // Read section headers
    file.seekg(ehdr.e_shoff);
    std::vector<Elf64_Shdr> shdrs(ehdr.e_shnum);
    file.read(reinterpret_cast<char*>(shdrs.data()),
              sizeof(Elf64_Shdr) * ehdr.e_shnum);

    // Find the string table for section names
    Elf64_Shdr& shstrtab = shdrs[ehdr.e_shstrndx];
    std::vector<char> shstrtab_data(shstrtab.sh_size);
    file.seekg(shstrtab.sh_offset);
    file.read(shstrtab_data.data(), shstrtab.sh_size);

    // Find .dynsym and .dynstr sections
    const Elf64_Shdr* dynsym = nullptr;
    const Elf64_Shdr* dynstr = nullptr;

    for (const auto& shdr : shdrs) {
        const char* name = shstrtab_data.data() + shdr.sh_name;
        if (std::strcmp(name, ".dynsym") == 0) {
            dynsym = &shdr;
        } else if (std::strcmp(name, ".dynstr") == 0) {
            dynstr = &shdr;
        }
    }

    if (dynsym && dynstr) {
        // Read dynamic symbol table
        std::vector<Elf64_Sym> symbols(dynsym->sh_size / sizeof(Elf64_Sym));
        file.seekg(dynsym->sh_offset);
        file.read(reinterpret_cast<char*>(symbols.data()), dynsym->sh_size);

        // Read string table
        std::vector<char> strtab_data(dynstr->sh_size);
        file.seekg(dynstr->sh_offset);
        file.read(strtab_data.data(), dynstr->sh_size);

        // Count type_info symbols
        for (const auto& sym : symbols) {
            if (sym.st_name < strtab_data.size()) {
                const char* sym_name = strtab_data.data() + sym.st_name;
                // Check for typeinfo symbols (typically start with _ZTI)
                if (std::strstr(sym_name, "_ZTI") == sym_name) {
                    count++;
                }
            }
        }
    }

    return count > 0;
}

// Scan directories recursively for shared libraries
void scan_directory(const fs::path& dir, std::vector<library_info>& libraries) {
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (fs::is_regular_file(entry.path())) {
                const std::string path = entry.path().string();
                const std::string ext = entry.path().extension().string();

                // Check for .so files
                if (ext == ".so" || path.find(".so.") != std::string::npos) {
                    size_t count = 0;
                    if (contains_type_info(path, count)) {
                        libraries.push_back({path, count});
                        std::cout << "Found: " << path << " (" << count << " type_info objects)" << std::endl;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error scanning " << dir << ": " << e.what() << std::endl;
    }
}

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS] PATH [PATH...]\n"
              << "\nSearch directories recursively for shared libraries containing type_info objects.\n"
              << "\nOptions:\n"
              << "  -o, --output FILE    Output JSON file (default: libraries.json)\n"
              << "  -h, --help          Show this help message\n"
              << "\nArguments:\n"
              << "  PATH                One or more directories to search recursively\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::string output_file = "libraries.json";
    std::vector<std::string> search_paths;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                std::cerr << "Error: " << arg << " requires an argument" << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        } else {
            search_paths.push_back(arg);
        }
    }

    if (search_paths.empty()) {
        std::cerr << "Error: No search paths specified" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    std::vector<library_info> libraries;

    std::cout << "Scanning for shared libraries containing type_info objects..." << std::endl;

    for (const auto& path : search_paths) {
        std::cout << "Scanning: " << path << std::endl;
        scan_directory(path, libraries);
    }

    // Sort by typeinfo count (descending)
    std::sort(libraries.begin(), libraries.end(),
              [](const library_info& a, const library_info& b) {
                  return a.typeinfo_count > b.typeinfo_count;
              });

    // Create JSON array
    json::array lib_array;
    for (const auto& lib : libraries) {
        json::object obj;
        obj["path"] = lib.path;
        obj["typeinfo_count"] = lib.typeinfo_count;
        lib_array.push_back(obj);
    }

    // Write to file
    std::ofstream out(output_file);
    if (!out) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return 1;
    }

    out << json::serialize(lib_array) << std::endl;

    std::cout << "\nFound " << libraries.size() << " libraries with type_info objects" << std::endl;
    std::cout << "Results written to: " << output_file << std::endl;

    return 0;
}
