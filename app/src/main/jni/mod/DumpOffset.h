#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <elf.h>

// ── Dump Lib ──────────────────────────────────────────────────────────────────
// Scan library yang ter-load oleh proses game lalu dump ELF symbol table-nya.
// Pendekatan ini TIDAK mengakses game objects sama sekali sehingga aman dipakai
// kapan pun — baik dalam match maupun di lobby.
// ─────────────────────────────────────────────────────────────────────────────

namespace DumpOffset {

    // ── Data per library ──────────────────────────────────────────────────────
    struct LibInfo {
        char     shortName[64];   // "libmain"
        char     fullPath[256];   // "/data/.../libmain.so"
        uintptr_t base;           // base address dari /proc/self/maps
    };

    // ── State ─────────────────────────────────────────────────────────────────
    static std::vector<LibInfo>    s_libs;
    static int                     s_selLib      = 0;
    static std::vector<std::string> s_symbols;
    static std::string             s_dumpText;
    static bool                    s_dumpReady   = false;
    static bool                    s_scanning    = false;
    static char                    s_saveStatus[128] = {};
    static bool                    s_libsLoaded  = false;

    // ── Safe range check (agar baca ELF tidak SIGSEGV) ───────────────────────
    static inline bool AddrOk(uintptr_t a, uintptr_t base, size_t span) {
        return a >= base && a < base + span;
    }

    // ── Ambil short name dari path ────────────────────────────────────────────
    static void MakeShortName(const char* path, char* out, int outLen) {
        const char* slash = strrchr(path, '/');
        const char* name  = slash ? slash + 1 : path;
        const char* dot   = strrchr(name, '.');
        int len = dot ? (int)(dot - name) : (int)strlen(name);
        if (len >= outLen) len = outLen - 1;
        memcpy(out, name, len);
        out[len] = '\0';
    }

    // ── Scan /proc/self/maps untuk daftar .so yang sedang ter-load ───────────
    static void ScanLibraries() {
        s_libs.clear();
        s_libsLoaded = false;

        FILE* f = fopen("/proc/self/maps", "r");
        if (!f) return;

        char line[512];
        while (fgets(line, sizeof(line), f)) {
            // Hanya baris yang punya path .so dan permission 'r-xp'
            if (!strstr(line, ".so") || !strstr(line, "r-xp")) continue;

            uintptr_t addrStart = 0;
            char perms[8] = {};
            unsigned long offset = 0;
            unsigned int dev_major = 0, dev_minor = 0;
            unsigned long inode = 0;
            char path[256] = {};

            int n = sscanf(line, "%lx-%*lx %7s %lx %x:%x %lu %255s",
                           &addrStart, perms, &offset, &dev_major, &dev_minor, &inode, path);
            if (n < 7 || path[0] == '\0') continue;
            if (!strstr(path, ".so"))     continue;
            // Hanya sertakan library yang kemungkinan relevan
            // (skip libs sistem yang terlalu banyak)
            const char* fn = strrchr(path, '/');
            if (!fn) continue;
            fn++;
            // Masukkan semua .so — user bisa pilih mana yg mau di-dump
            // Hindari duplikat (ambil mapping pertama = base)
            bool dup = false;
            for (auto& ex : s_libs) {
                if (strcmp(ex.fullPath, path) == 0) { dup = true; break; }
            }
            if (dup) continue;

            LibInfo li{};
            li.base = addrStart;
            strncpy(li.fullPath, path, sizeof(li.fullPath) - 1);
            MakeShortName(path, li.shortName, sizeof(li.shortName));
            s_libs.push_back(li);

            if ((int)s_libs.size() >= 64) break; // cukup 64 lib
        }
        fclose(f);
        s_libsLoaded = true;
        // Clamp selection
        if (s_selLib >= (int)s_libs.size()) s_selLib = 0;
    }

    // ── Hitung ukuran mapping library di memori ───────────────────────────────
    static size_t GetLibraryMappedSize(uintptr_t base) {
        size_t total = 0;
        FILE* f = fopen("/proc/self/maps", "r");
        if (!f) return 0x4000000; // default 64 MB fallback

        char line[512];
        while (fgets(line, sizeof(line), f)) {
            uintptr_t s = 0, e = 0;
            char path[256] = {};
            sscanf(line, "%lx-%lx %*s %*s %*s %*s %255s", &s, &e, path);
            // Temukan semua segment yang termasuk library ini
            // Cukup ambil range terbesar dari base
            if (s >= base && (e - base) > total) {
                total = e - base;
            }
        }
        fclose(f);
        return total > 0 ? total : 0x4000000;
    }

    // ── Scan ELF symbol table dari library yang dipilih ───────────────────────
    static void ScanSymbols() {
        s_symbols.clear();
        s_dumpReady = false;
        s_dumpText.clear();

        if (s_libs.empty() || s_selLib < 0 || s_selLib >= (int)s_libs.size()) {
            s_saveStatus[0] = '\0';
            snprintf(s_saveStatus, sizeof(s_saveStatus), "Pilih library dulu!");
            return;
        }

        const LibInfo& lib = s_libs[s_selLib];
        uintptr_t base = lib.base;
        size_t    span = GetLibraryMappedSize(base);

        // Validasi ELF magic di base address
        if (!AddrOk(base, base, span) || span < sizeof(Elf64_Ehdr)) return;
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)base;
        if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
            ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
            ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
            ehdr->e_ident[EI_MAG3] != ELFMAG3) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "ELF magic tidak valid!");
            return;
        }

        // Cari PT_DYNAMIC segment
        Elf64_Phdr* phdr = nullptr;
        if (AddrOk(base + ehdr->e_phoff, base, span)) {
            phdr = (Elf64_Phdr*)(base + ehdr->e_phoff);
        } else {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "PHDR tidak aman!");
            return;
        }

        uintptr_t dynAddr = 0;
        size_t    dynSize = 0;
        for (int i = 0; i < (int)ehdr->e_phnum; i++) {
            if (!AddrOk((uintptr_t)&phdr[i], base, span)) break;
            if (phdr[i].p_type == PT_DYNAMIC) {
                dynAddr = base + phdr[i].p_vaddr;
                dynSize = phdr[i].p_filesz;
                break;
            }
        }
        if (!dynAddr || !AddrOk(dynAddr, base, span)) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "Tidak ada PT_DYNAMIC!");
            return;
        }

        // Parse PT_DYNAMIC untuk DT_SYMTAB, DT_STRTAB, DT_STRSZ
        Elf64_Dyn* dyn    = (Elf64_Dyn*)dynAddr;
        uintptr_t symtab  = 0;
        uintptr_t strtab  = 0;
        size_t    strsz   = 0;
        size_t    syment  = sizeof(Elf64_Sym);

        for (int i = 0; ; i++) {
            uintptr_t dynEntry = dynAddr + i * sizeof(Elf64_Dyn);
            if (!AddrOk(dynEntry + sizeof(Elf64_Dyn), base, span)) break;
            Elf64_Dyn* d = (Elf64_Dyn*)dynEntry;
            if (d->d_tag == DT_NULL) break;
            if (d->d_tag == DT_SYMTAB)  symtab = base + d->d_un.d_ptr;
            if (d->d_tag == DT_STRTAB)  strtab = base + d->d_un.d_ptr;
            if (d->d_tag == DT_STRSZ)   strsz  = d->d_un.d_val;
            if (d->d_tag == DT_SYMENT)  syment = d->d_un.d_val;
        }

        if (!symtab || !strtab || !strsz) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "DT_SYMTAB/STRTAB tidak ada!");
            return;
        }
        if (!AddrOk(symtab, base, span) || !AddrOk(strtab, base, span)) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "SYMTAB addr tidak aman!");
            return;
        }

        // Hitung jumlah simbol: symtab berjalan sampai strtab
        size_t maxSyms = (strtab > symtab) ? (strtab - symtab) / syment : 0;
        if (maxSyms > 65536) maxSyms = 65536;

        // Kumpulkan simbol
        std::ostringstream ss;
        ss << "// ================================================================\n";
        ss << "// Flux Pro Engine v2.0 -- ELF Symbol Dump\n";
        ss << "// Library  : " << lib.shortName << "\n";
        ss << "// Path     : " << lib.fullPath  << "\n";
        char hexbuf[32];
        snprintf(hexbuf, sizeof(hexbuf), "0x%lX", (unsigned long)lib.base);
        ss << "// Base     : " << hexbuf        << "\n";
        ss << "// Arch     : arm64-v8a / ELF64\n";
        ss << "// Build    : " << __DATE__ << " " << __TIME__ << "\n";
        ss << "// ================================================================\n\n";

        int found = 0;
        for (size_t i = 1; i < maxSyms; i++) {
            uintptr_t symAddr = symtab + i * syment;
            if (!AddrOk(symAddr + sizeof(Elf64_Sym), base, span)) break;

            Elf64_Sym* sym = (Elf64_Sym*)symAddr;
            if (sym->st_name == 0) continue;

            uintptr_t nameAddr = strtab + sym->st_name;
            if (!AddrOk(nameAddr, base, span)) continue;

            const char* name = (const char*)nameAddr;
            // Validasi string: ASCII printable
            bool ok = true;
            int  len = 0;
            while (len < 256) {
                if (!AddrOk(nameAddr + len, base, span)) { ok = false; break; }
                char c = name[len];
                if (c == 0) break;
                if ((unsigned char)c < 0x20) { ok = false; break; }
                len++;
            }
            if (!ok || len == 0) continue;

            uintptr_t rva = (sym->st_value > 0) ? sym->st_value : 0;
            char linebuf[384];
            snprintf(linebuf, sizeof(linebuf), "// [%5zu] +0x%08lX  %s\n",
                     i, (unsigned long)rva, name);
            ss << linebuf;
            s_symbols.push_back(name);
            found++;
        }

        ss << "\n// Total: " << found << " symbols\n";
        s_dumpText = ss.str();
        s_dumpReady = true;
        snprintf(s_saveStatus, sizeof(s_saveStatus), "Found %d symbols", found);
    }

    // ── Entry point: scan libs lalu langsung scan symbols ─────────────────────
    static void RunScan() {
        if (s_scanning) return;
        s_scanning = true;
        s_dumpReady = false;
        s_saveStatus[0] = '\0';
        if (!s_libsLoaded) ScanLibraries();
        ScanSymbols();
        s_scanning = false;
    }

    // ── Refresh daftar library saja (tanpa scan symbol) ───────────────────────
    static void RefreshLibs() {
        s_libsLoaded = false;
        s_symbols.clear();
        s_dumpText.clear();
        s_dumpReady = false;
        ScanLibraries();
    }

    // ── Simpan dump ke file ───────────────────────────────────────────────────
    static void SaveToFile() {
        if (!s_dumpReady || s_dumpText.empty()) {
            snprintf(s_saveStatus, sizeof(s_saveStatus), "Scan dulu sebelum save!");
            return;
        }

        const char* dirs[] = {
            "/storage/emulated/0/Android/data/com.miniclip.eightballpool/files/Dump",
            "/data/data/com.miniclip.eightballpool/files/Dump",
            "/storage/emulated/0/Download",
            "/sdcard/Download",
        };

        char fname[64] = "dump_lib";
        if (!s_libs.empty() && s_selLib < (int)s_libs.size()) {
            snprintf(fname, sizeof(fname), "dump_%s", s_libs[s_selLib].shortName);
        }

        for (auto& dir : dirs) {
            create_directory_recursive(dir);
            char fpath[320];
            snprintf(fpath, sizeof(fpath), "%s/%s.txt", dir, fname);
            std::ofstream ofs(fpath, std::ios::out | std::ios::trunc);
            if (ofs.is_open()) {
                ofs << s_dumpText;
                ofs.close();
                snprintf(s_saveStatus, sizeof(s_saveStatus), "Saved: %s", fpath);
                LOGI("DumpOffset: saved to %s", fpath);
                return;
            }
        }
        snprintf(s_saveStatus, sizeof(s_saveStatus), "Gagal menyimpan file!");
    }

} // namespace DumpOffset
