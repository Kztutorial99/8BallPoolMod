#pragma once

#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cctype>
#include <fstream>
#include <thread>
#include <atomic>

#include "../include/manual_dlsym.h"
#include "../include/includes.h"

namespace DumpLib {

// ── Structs ──────────────────────────────────────────────────────────────────

struct SymEntry {
    std::string name;
    uintptr_t   offset;     // relative to lib base
    uint8_t     type;       // STT_FUNC, STT_OBJECT, STT_NOTYPE, etc.
    uint64_t    size;
    bool        from_file;  // true = from .symtab on disk, false = from memory
};

struct PatternResult {
    uintptr_t offset;
    std::string ctx;
};

// ── State ─────────────────────────────────────────────────────────────────────

static std::vector<SymEntry>      g_symbols;
static std::vector<PatternResult> g_patResults;
static std::string                g_dumpStatus;
static std::atomic<bool>          g_scanning(false);
static std::atomic<bool>          g_patScanning(false);

static char g_filterBuf[128]  = {};
static char g_patternBuf[256] = {};
static int  g_libSel          = 0;  // 0=libmain, 1=libanogs, 2=libintl
static int  g_typeSel         = 0;  // 0=All, 1=FUNC, 2=OBJECT, 3=NOTYPE

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* sym_type_str(uint8_t t) {
    switch (t) {
        case STT_FUNC:    return "FUNC";
        case STT_OBJECT:  return "OBJ";
        case STT_NOTYPE:  return "NONE";
        case STT_SECTION: return "SEC";
        case STT_FILE:    return "FILE";
        default:          return "???";
    }
}

static uintptr_t selected_base() {
    switch (g_libSel) {
        case 1:  return libanogs;
        case 2:  return libintl;
        default: return libmain;
    }
}

static const char* selected_lib_name() {
    switch (g_libSel) {
        case 1:  return "libanogs";
        case 2:  return "libintl";
        default: return "libmain";
    }
}

// ── ELF file parser — reads ALL symbols from disk (.dynsym + .symtab) ─────────

static void scan_elf_file(uintptr_t base, const std::string& path,
                          std::vector<SymEntry>& out) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(ElfW(Ehdr))) {
        close(fd);
        return;
    }

    std::vector<uint8_t> data(st.st_size);
    if (read(fd, data.data(), st.st_size) != st.st_size) {
        close(fd);
        return;
    }
    close(fd);

    auto* ehdr = reinterpret_cast<ElfW(Ehdr)*>(data.data());
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') return;

    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0) return;
    if ((size_t)(ehdr->e_shoff + ehdr->e_shnum * sizeof(ElfW(Shdr))) > data.size()) return;

    auto* shdrs = reinterpret_cast<ElfW(Shdr)*>(data.data() + ehdr->e_shoff);

    // Locate .shstrtab
    if (ehdr->e_shstrndx >= ehdr->e_shnum) return;
    auto& shstrhdr = shdrs[ehdr->e_shstrndx];
    if (shstrhdr.sh_offset + shstrhdr.sh_size > (uint64_t)data.size()) return;
    const char* shstrtab = reinterpret_cast<const char*>(data.data() + shstrhdr.sh_offset);

    // Process each section — collect (symtab, strtab) pairs for both .dynsym and .symtab
    struct SymSec { uint64_t sym_off; uint64_t sym_sz; uint64_t str_off; uint64_t str_sz; };
    std::vector<SymSec> sections;

    // Collect all SHT_SYMTAB / SHT_DYNSYM sections with their linked strtabs
    for (int i = 0; i < ehdr->e_shnum; i++) {
        auto& sh = shdrs[i];
        if (sh.sh_type != SHT_SYMTAB && sh.sh_type != SHT_DYNSYM) continue;
        if (sh.sh_link == 0 || sh.sh_link >= ehdr->e_shnum) continue;
        auto& strsh = shdrs[sh.sh_link];
        if (sh.sh_offset + sh.sh_size > (uint64_t)data.size()) continue;
        if (strsh.sh_offset + strsh.sh_size > (uint64_t)data.size()) continue;
        sections.push_back({ sh.sh_offset, sh.sh_size, strsh.sh_offset, strsh.sh_size });
    }

    for (auto& sec : sections) {
        auto* syms   = reinterpret_cast<ElfW(Sym)*>(data.data() + sec.sym_off);
        auto* strtab = reinterpret_cast<const char*>(data.data() + sec.str_off);
        uint64_t count = sec.sym_sz / sizeof(ElfW(Sym));

        for (uint64_t j = 0; j < count; j++) {
            auto& sym = syms[j];
            uint8_t stype = ELF_ST_TYPE(sym.st_info);
            if (stype == STT_FILE || stype == STT_SECTION) continue;
            if (sym.st_value == 0) continue;
            if (sym.st_name == 0) continue;

            uint64_t name_off = sym.st_name;
            if (name_off >= sec.str_sz) continue;
            const char* sname = strtab + name_off;
            if (*sname == '\0') continue;

            SymEntry e;
            e.name      = sname;
            e.offset    = (uintptr_t)sym.st_value;  // file offset = relative offset for PIE
            e.type      = stype;
            e.size      = sym.st_size;
            e.from_file = true;
            out.push_back(std::move(e));
        }
    }
}

// ── Memory scanner — exported symbols from running process ───────────────────

static void scan_exported(uintptr_t base, std::vector<SymEntry>& out) {
    if (!base) return;
    ManualLookup::walk_exported_symbols(base, [&](const char* name, void* addr) -> bool {
        SymEntry e;
        e.name      = name ? name : "";
        e.offset    = (uintptr_t)addr - base;
        e.type      = STT_FUNC;  // exported = assumed func unless we can check
        e.size      = 0;
        e.from_file = false;
        out.push_back(std::move(e));
        return false;
    });
}

// ── Deduplicate by offset, prefer file entries ────────────────────────────────

static void dedup(std::vector<SymEntry>& syms) {
    // Sort by offset then name
    std::sort(syms.begin(), syms.end(), [](const SymEntry& a, const SymEntry& b) {
        if (a.offset != b.offset) return a.offset < b.offset;
        // file-based entries preferred (they have more info)
        return (int)b.from_file < (int)a.from_file;
    });
    // Remove duplicates with same name + offset
    auto it = std::unique(syms.begin(), syms.end(), [](const SymEntry& a, const SymEntry& b) {
        return a.offset == b.offset && a.name == b.name;
    });
    syms.erase(it, syms.end());
}

// ── Pattern scan ──────────────────────────────────────────────────────────────

static bool parse_pattern(const char* pat_str,
                          std::vector<uint8_t>& bytes,
                          std::vector<bool>& mask) {
    bytes.clear(); mask.clear();
    const char* p = pat_str;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (*p == '?' && (*(p+1) == '?' || *(p+1) == ' ' || *(p+1) == '\0')) {
            bytes.push_back(0);
            mask.push_back(false);
            p++; if (*p == '?') p++;
        } else {
            char hex[3] = { p[0], p[1], '\0' };
            if (!isxdigit((unsigned char)hex[0]) || !isxdigit((unsigned char)hex[1])) return false;
            bytes.push_back((uint8_t)strtol(hex, nullptr, 16));
            mask.push_back(true);
            p += 2;
        }
    }
    return !bytes.empty();
}

static void do_pattern_scan(uintptr_t base, size_t scan_len,
                            const std::vector<uint8_t>& bytes,
                            const std::vector<bool>& mask,
                            std::vector<PatternResult>& results,
                            int max_results = 50) {
    if (!base || bytes.empty() || scan_len < bytes.size()) return;

    size_t pat_sz = bytes.size();
    auto* mem = reinterpret_cast<const uint8_t*>(base);

    for (size_t i = 0; i <= scan_len - pat_sz; i++) {
        bool match = true;
        for (size_t j = 0; j < pat_sz; j++) {
            if (mask[j] && mem[i + j] != bytes[j]) { match = false; break; }
        }
        if (match) {
            PatternResult r;
            r.offset = (uintptr_t)i;
            char buf[64];
            snprintf(buf, sizeof(buf), "0x%lx", (unsigned long)i);
            r.ctx = buf;
            results.push_back(r);
            if ((int)results.size() >= max_results) break;
        }
    }
}

// ── Dump to file ──────────────────────────────────────────────────────────────

static bool dump_to_file(const std::vector<SymEntry>& syms,
                         const std::string& lib_name,
                         uintptr_t base) {
    std::string path = std::string("/sdcard/") + lib_name + "_dump.txt";
    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        // fallback to /data/local/tmp
        path = std::string("/data/local/tmp/") + lib_name + "_dump.txt";
        f = fopen(path.c_str(), "w");
    }
    if (!f) return false;

    fprintf(f, "# DumpLib — %s\n", lib_name.c_str());
    fprintf(f, "# Base: 0x%lx\n", (unsigned long)base);
    fprintf(f, "# Total symbols: %zu\n\n", syms.size());
    fprintf(f, "%-16s  %-8s  %-6s  %s\n", "Offset", "Size", "Type", "Name");
    fprintf(f, "%-16s  %-8s  %-6s  %s\n",
            "----------------", "--------", "------", "--------------------");

    for (auto& e : syms) {
        fprintf(f, "0x%014lx  %-8lu  %-6s  %s\n",
                (unsigned long)e.offset,
                (unsigned long)e.size,
                sym_type_str(e.type),
                e.name.c_str());
    }
    fclose(f);
    g_dumpStatus = "Saved: " + path;
    return true;
}

// ── Async scan launcher ───────────────────────────────────────────────────────

static void launch_scan() {
    if (g_scanning) return;
    g_scanning = true;
    g_dumpStatus = "Scanning...";

    std::thread([]{
        uintptr_t base = selected_base();
        std::string libname = selected_lib_name();

        std::vector<SymEntry> tmp;

        // 1. Memory scan (exported)
        scan_exported(base, tmp);

        // 2. ELF file scan (all symbols)
        std::string fpath = lpath(libname + std::string(".so"));
        if (!fpath.empty()) {
            scan_elf_file(base, fpath, tmp);
        }

        dedup(tmp);

        g_symbols = std::move(tmp);

        char buf[64];
        snprintf(buf, sizeof(buf), "Found %zu symbols", g_symbols.size());
        g_dumpStatus = buf;
        g_scanning = false;
    }).detach();
}

static void launch_pattern_scan() {
    if (g_patScanning || g_patternBuf[0] == '\0') return;
    g_patScanning = true;

    std::string pat_copy = g_patternBuf;
    std::thread([pat_copy]{
        std::vector<uint8_t> bytes;
        std::vector<bool>    mask;
        if (!parse_pattern(pat_copy.c_str(), bytes, mask)) {
            g_dumpStatus = "Invalid pattern";
            g_patScanning = false;
            return;
        }

        uintptr_t base = selected_base();
        // Estimate scan range from /proc/self/maps
        uintptr_t scan_end = base;
        {
            std::ifstream maps("/proc/self/maps");
            std::string line;
            while (std::getline(maps, line)) {
                if (line.find(selected_lib_name()) == std::string::npos) continue;
                // Parse end address
                size_t dash = line.find('-');
                if (dash == std::string::npos) continue;
                size_t space = line.find(' ', dash);
                if (space == std::string::npos) continue;
                uintptr_t end = (uintptr_t)strtoul(line.substr(dash + 1, space - dash - 1).c_str(), nullptr, 16);
                if (end > scan_end) scan_end = end;
            }
        }

        size_t scan_len = (scan_end > base) ? (scan_end - base) : 0;
        std::vector<PatternResult> res;
        if (scan_len > 0)
            do_pattern_scan(base, scan_len, bytes, mask, res);

        g_patResults = std::move(res);

        char buf[64];
        snprintf(buf, sizeof(buf), "Pattern: %zu match(es)", g_patResults.size());
        g_dumpStatus = buf;
        g_patScanning = false;
    }).detach();
}

// ── ImGui UI ──────────────────────────────────────────────────────────────────

INLINE void DrawUI() {
    using namespace ImGui;

    Dummy(ImVec2(0, 8));

    // ── Library selector ────────────────────────────────────────────────────
    SetWindowFontScale(0.85f);
    TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Library");
    SetWindowFontScale(1.0f);
    Dummy(ImVec2(0, 4));

    PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.26f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.12f, 0.12f, 1.0f));

    float btnW3 = (GetContentRegionAvail().x - 8.0f) / 3.0f;
    const char* libs[] = { "libmain", "libanogs", "libintl" };
    for (int i = 0; i < 3; i++) {
        if (i > 0) SameLine(0, 4);
        bool sel = (g_libSel == i);
        if (sel) PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (Button(libs[i], ImVec2(btnW3, 40.0f))) { g_libSel = i; g_symbols.clear(); g_dumpStatus = ""; }
        if (sel) PopStyleColor();
    }
    PopStyleColor(3);
    PopStyleVar();

    Dummy(ImVec2(0, 8));

    // ── Scan + Dump buttons ──────────────────────────────────────────────────
    float halfW = (GetContentRegionAvail().x - 6.0f) * 0.5f;
    PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

    PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.45f, 0.72f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.55f, 0.86f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.36f, 0.60f, 1.0f));
    if (Button(g_scanning ? "Scanning..." : "Scan Symbols", ImVec2(halfW, 48.0f))) {
        if (!g_scanning) launch_scan();
    }
    PopStyleColor(3);

    SameLine(0, 6);

    PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.48f, 0.22f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.60f, 0.28f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.09f, 0.36f, 0.17f, 1.0f));
    if (Button("Dump to File", ImVec2(halfW, 48.0f))) {
        if (!g_symbols.empty())
            dump_to_file(g_symbols, selected_lib_name(), selected_base());
        else
            g_dumpStatus = "Scan first!";
    }
    PopStyleColor(3);
    PopStyleVar();

    // ── Status ───────────────────────────────────────────────────────────────
    if (!g_dumpStatus.empty()) {
        Dummy(ImVec2(0, 4));
        TextColored(ImVec4(0.4f, 1.0f, 0.55f, 1.0f), "%s", g_dumpStatus.c_str());
    }

    Dummy(ImVec2(0, 10));

    // ── Type filter ─────────────────────────────────────────────────────────
    SetWindowFontScale(0.85f);
    TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Type Filter");
    SetWindowFontScale(1.0f);
    Dummy(ImVec2(0, 4));

    PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    PushStyleColor(ImGuiCol_Button,        ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.26f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.12f, 0.12f, 1.0f));
    const char* types[] = { "All", "FUNC", "OBJ", "NONE" };
    float btnW4 = (GetContentRegionAvail().x - 12.0f) / 4.0f;
    for (int i = 0; i < 4; i++) {
        if (i > 0) SameLine(0, 4);
        bool sel = (g_typeSel == i);
        if (sel) PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
        if (Button(types[i], ImVec2(btnW4, 36.0f))) g_typeSel = i;
        if (sel) PopStyleColor();
    }
    PopStyleColor(3);
    PopStyleVar();

    Dummy(ImVec2(0, 8));

    // ── Name search ──────────────────────────────────────────────────────────
    SetWindowFontScale(0.85f);
    TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Search by Name");
    SetWindowFontScale(1.0f);
    Dummy(ImVec2(0, 4));

    PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
    SetNextItemWidth(GetContentRegionAvail().x);
    InputText("##filter", g_filterBuf, sizeof(g_filterBuf));
    PopStyleColor();
    PopStyleVar();

    Dummy(ImVec2(0, 8));

    // ── Symbol list ──────────────────────────────────────────────────────────
    if (!g_symbols.empty()) {
        // Apply filters
        std::string filter_lo = g_filterBuf;
        std::transform(filter_lo.begin(), filter_lo.end(), filter_lo.begin(), ::tolower);

        const uint8_t type_map[] = { 255, STT_FUNC, STT_OBJECT, STT_NOTYPE };
        uint8_t want_type = type_map[g_typeSel];

        // Count matching
        int shown = 0;
        for (auto& e : g_symbols) {
            if (want_type != 255 && e.type != want_type) continue;
            if (!filter_lo.empty()) {
                std::string nl = e.name;
                std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
                if (nl.find(filter_lo) == std::string::npos) continue;
            }
            shown++;
        }

        char hdr[64];
        snprintf(hdr, sizeof(hdr), "Symbols: %d / %zu", shown, g_symbols.size());
        SetWindowFontScale(0.85f);
        TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f), "%s", hdr);
        SetWindowFontScale(1.0f);
        Dummy(ImVec2(0, 4));

        PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));
        PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        float listH = GetContentRegionAvail().y - 160.0f;
        if (listH < 80.0f) listH = 80.0f;
        BeginChild("##symlist", ImVec2(GetContentRegionAvail().x, listH), false);

        SetWindowFontScale(0.80f);
        for (auto& e : g_symbols) {
            if (want_type != 255 && e.type != want_type) continue;
            if (!filter_lo.empty()) {
                std::string nl = e.name;
                std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
                if (nl.find(filter_lo) == std::string::npos) continue;
            }

            char row[256];
            if (e.size > 0)
                snprintf(row, sizeof(row), "[0x%lx] %s (%s, %lub)",
                    (unsigned long)e.offset, e.name.c_str(),
                    sym_type_str(e.type), (unsigned long)e.size);
            else
                snprintf(row, sizeof(row), "[0x%lx] %s (%s)",
                    (unsigned long)e.offset, e.name.c_str(), sym_type_str(e.type));

            ImVec4 col = (e.type == STT_FUNC)
                ? ImVec4(0.55f, 0.85f, 1.00f, 1.0f)
                : (e.type == STT_OBJECT)
                    ? ImVec4(0.85f, 0.75f, 0.40f, 1.0f)
                    : ImVec4(0.75f, 0.75f, 0.80f, 1.0f);

            TextColored(col, "%s", row);
        }
        SetWindowFontScale(1.0f);
        EndChild();
        PopStyleVar();
        PopStyleColor();
    } else {
        Dummy(ImVec2(0, 10));
        float w = GetContentRegionAvail().x;
        ImVec2 ts = CalcTextSize("Press  Scan Symbols  to start");
        SetCursorPosX((w - ts.x) * 0.5f);
        TextColored(ImVec4(0.4f, 0.4f, 0.45f, 1.0f), "Press  Scan Symbols  to start");
    }

    Dummy(ImVec2(0, 12));

    // ── Pattern Scan section ─────────────────────────────────────────────────
    SetWindowFontScale(0.85f);
    TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Pattern Scan  (e.g.  FF 03 ?? 91 ?? ?? ?? 94)");
    SetWindowFontScale(1.0f);
    Dummy(ImVec2(0, 4));

    float pscanW = GetContentRegionAvail().x;
    float inpW   = pscanW - 120.0f;

    PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
    SetNextItemWidth(inpW);
    InputText("##pat", g_patternBuf, sizeof(g_patternBuf));
    PopStyleColor();

    SameLine(0, 6);

    PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.20f, 0.55f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.28f, 0.68f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.42f, 0.14f, 0.42f, 1.0f));
    if (Button(g_patScanning ? "..." : "Scan", ImVec2(114.0f, 0))) {
        if (!g_patScanning) launch_pattern_scan();
    }
    PopStyleColor(3);
    PopStyleVar();

    if (!g_patResults.empty()) {
        Dummy(ImVec2(0, 6));
        PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));
        PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        BeginChild("##patlist", ImVec2(GetContentRegionAvail().x, 130.0f), false);
        SetWindowFontScale(0.80f);
        for (auto& r : g_patResults) {
            char row[64];
            snprintf(row, sizeof(row), "  [0x%lx]", (unsigned long)r.offset);
            TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", row);
        }
        SetWindowFontScale(1.0f);
        EndChild();
        PopStyleVar();
        PopStyleColor();
    }
}

} // namespace DumpLib
