#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/core/demangle.hpp>
#include <boost/program_options.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <elf.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlfcn.h>

namespace fs = boost::filesystem;
namespace json = boost::json;
namespace po = boost::program_options;
namespace dll = boost::dll;

struct library_info {
    std::string path;
    size_t typeinfo_count;
};

struct typeinfo_data {
    void* address;
    std::string name;
};

// Generate combinations of libraries
void generate_combinations(const std::vector<library_info>& libraries,
                          size_t max_size,
                          std::vector<std::vector<std::string>>& combinations) {
    combinations.clear();

    // Generate all combinations from size 1 to max_size
    for (size_t size = 1; size <= max_size && size <= libraries.size(); ++size) {
        std::vector<size_t> indices(size);
        for (size_t i = 0; i < size; ++i) {
            indices[i] = i;
        }

        // Generate combinations using indices
        while (true) {
            std::vector<std::string> combination;
            for (size_t idx : indices) {
                combination.push_back(libraries[idx].path);
            }
            combinations.push_back(combination);

            // Next combination
            int pos = size - 1;
            while (pos >= 0 && indices[pos] == libraries.size() - size + pos) {
                --pos;
            }

            if (pos < 0) break;

            ++indices[pos];
            for (size_t i = pos + 1; i < size; ++i) {
                indices[i] = indices[i - 1] + 1;
            }
        }
    }
}

// Collect type_info objects from loaded libraries
std::vector<typeinfo_data> collect_typeinfo_objects(const std::vector<std::string>& lib_paths) {
    std::vector<typeinfo_data> result;
    std::vector<dll::shared_library> libraries;

    // Load all libraries using Boost.DLL
    for (const auto& path : lib_paths) {
        try {
            boost::system::error_code ec;
            dll::shared_library lib(path, dll::load_mode::rtld_lazy | dll::load_mode::rtld_global, ec);
            if (!ec && lib.is_loaded()) {
                libraries.push_back(std::move(lib));
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load " << path << ": " << e.what() << std::endl;
        }
    }

    // For each library, read its ELF symbols and find type_info
    for (const auto& path : lib_paths) {
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;

        // Read ELF header
        Elf64_Ehdr ehdr;
        file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

        if (file.gcount() != sizeof(ehdr) ||
            std::memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
            continue;
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

            // Find type_info symbols
            for (const auto& sym : symbols) {
                if (sym.st_name < strtab_data.size()) {
                    const char* sym_name = strtab_data.data() + sym.st_name;
                    // Check for typeinfo symbols (typically start with _ZTI)
                    if (std::strstr(sym_name, "_ZTI") == sym_name) {
                        void* addr = dlsym(RTLD_DEFAULT, sym_name);
                        if (addr) {
                            std::string demangled = boost::core::demangle(sym_name);
                            result.push_back({addr, demangled});
                        }
                    }
                }
            }
        }
    }

    return result;
}

// Pretty-print JSON value with indentation
void write_json_pretty(std::ostream& os, const json::value& jv, int indent = 0) {
    std::string indent_str(indent * 2, ' ');
    std::string indent_str_inner((indent + 1) * 2, ' ');

    switch (jv.kind()) {
        case json::kind::object: {
            const auto& obj = jv.as_object();
            os << "{\n";
            auto it = obj.begin();
            for (; it != obj.end(); ++it) {
                os << indent_str_inner << "\"" << it->key() << "\": ";
                write_json_pretty(os, it->value(), indent + 1);
                auto next = it;
                ++next;
                if (next != obj.end()) {
                    os << ",";
                }
                os << "\n";
            }
            os << indent_str << "}";
            break;
        }
        case json::kind::array: {
            const auto& arr = jv.as_array();
            if (arr.empty()) {
                os << "[]";
            } else {
                os << "[\n";
                for (size_t i = 0; i < arr.size(); ++i) {
                    os << indent_str_inner;
                    write_json_pretty(os, arr[i], indent + 1);
                    if (i < arr.size() - 1) {
                        os << ",";
                    }
                    os << "\n";
                }
                os << indent_str << "]";
            }
            break;
        }
        case json::kind::string:
            os << json::serialize(jv.as_string());
            break;
        case json::kind::int64:
            os << jv.as_int64();
            break;
        case json::kind::uint64:
            os << jv.as_uint64();
            break;
        case json::kind::double_:
            os << jv.as_double();
            break;
        case json::kind::bool_:
            os << (jv.as_bool() ? "true" : "false");
            break;
        case json::kind::null:
            os << "null";
            break;
    }
}

// Process a combination in a separate process
bool process_combination(const std::vector<std::string>& libs,
                        const std::string& output_file,
                        bool human_readable) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        auto typeinfos = collect_typeinfo_objects(libs);

        // Sort typeinfos by address
        std::sort(typeinfos.begin(), typeinfos.end(),
                  [](const typeinfo_data& a, const typeinfo_data& b) {
                      return reinterpret_cast<uintptr_t>(a.address) <
                             reinterpret_cast<uintptr_t>(b.address);
                  });

        // Create JSON output
        json::object root;

        json::array lib_array;
        for (const auto& lib : libs) {
            lib_array.push_back(json::value(lib));
        }
        root["libraries"] = lib_array;

        json::array typeinfo_array;
        for (const auto& ti : typeinfos) {
            json::object ti_obj;

            std::ostringstream addr_stream;
            addr_stream << "0x" << std::hex << std::setw(16) << std::setfill('0')
                       << reinterpret_cast<uintptr_t>(ti.address);
            ti_obj["address"] = addr_stream.str();
            ti_obj["name"] = ti.name;

            typeinfo_array.push_back(ti_obj);
        }
        root["typeinfos"] = typeinfo_array;

        // Write to file
        std::ofstream out(output_file);
        if (out) {
            if (human_readable) {
                write_json_pretty(out, root);
                out << std::endl;
            } else {
                out << json::serialize(root) << std::endl;
            }
        }

        exit(0);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    } else {
        std::cerr << "Fork failed" << std::endl;
        return false;
    }
}

int main(int argc, char* argv[]) {
    std::string input_file = "libraries.json";
    size_t max_combination_size = 4;
    size_t max_libraries = 0;
    bool human_readable = false;

    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show help message")
        ("input,i", po::value<std::string>(&input_file)->default_value("libraries.json"),
         "Input JSON file from collect-libraries")
        ("max-size,n", po::value<size_t>(&max_combination_size)->default_value(4),
         "Maximum combination size")
        ("max-libs,m", po::value<size_t>(&max_libraries)->default_value(0),
         "Maximum number of libraries to consider (0 = all)")
        ("human-readable,H", po::bool_switch(&human_readable),
         "Format JSON output in human-readable form");

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    // Read input JSON
    std::ifstream input(input_file);
    if (!input) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        return 1;
    }

    std::string json_str((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());

    json::value parsed = json::parse(json_str);
    json::array lib_array = parsed.as_array();

    std::vector<library_info> libraries;
    for (const auto& item : lib_array) {
        json::object obj = item.as_object();
        library_info info;
        info.path = obj["path"].as_string().c_str();
        info.typeinfo_count = obj["typeinfo_count"].as_int64();
        libraries.push_back(info);
    }

    // Limit libraries if requested
    if (max_libraries > 0 && libraries.size() > max_libraries) {
        libraries.resize(max_libraries);
    }

    std::cout << "Processing " << libraries.size() << " libraries" << std::endl;
    std::cout << "Maximum combination size: " << max_combination_size << std::endl;

    // Generate combinations
    std::vector<std::vector<std::string>> combinations;
    generate_combinations(libraries, max_combination_size, combinations);

    std::cout << "Total combinations: " << combinations.size() << std::endl;

    // Process each combination
    size_t file_counter = 0;
    for (const auto& combo : combinations) {
        std::ostringstream filename;
        filename << std::setw(8) << std::setfill('0') << file_counter << ".json";

        std::cout << "Processing combination " << (file_counter + 1) << "/"
                  << combinations.size() << ": ";
        for (size_t i = 0; i < combo.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << fs::path(combo[i]).filename().string();
        }
        std::cout << std::endl;

        process_combination(combo, filename.str(), human_readable);
        ++file_counter;
    }

    std::cout << "\nProcessed " << file_counter << " combinations" << std::endl;

    return 0;
}
