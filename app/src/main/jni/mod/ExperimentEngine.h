#pragma once

// ════════════════════════════════════════════════════════════════════════
// EXPERIMENT ENGINE v4 — CRASH ANALYSIS & FIX
//
// ROOT CAUSE yang diperbaiki:
//   Tick() v3 memanggil GetCueBall() setiap frame tanpa pengecekan
//   apakah sedang in-game. GetCueBall() melakukan 4-level pointer
//   dereference (GM→Table→PNSArray→Ball), yang crash saat objek belum
//   valid (lobby, loading, transisi scene).
//
// FIX v4:
//   1. Tick(bool inGame) — terima flag g_espIsInGame dari DrawMenu
//   2. ZONA 1 (selalu aman): hanya akses libmain static constant
//   3. Early return jika !inGame — skip semua game-object access
//   4. ZONA 2 (hanya saat inGame): akses game object HANYA bila
//      experiment yang bersangkutan aktif (tidak untuk live readout)
//   5. GetCueBall() TIDAK PERNAH dipanggil unconditionally
// ════════════════════════════════════════════════════════════════════════

#include "../include/includes.h"
#include "../game/GameManager.h"
#include "../game/UserInfo.h"
#include "../game/CCDirector.h"
#include <sys/mman.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

namespace Experiment {

    // ── [1] mAimOffset ───────────────────────────────────────────────────
    static bool   bAimOffsetActive = false;
    static float  fAimOffsetX = 0.0f, fAimOffsetY = 0.0f;
    static double rdAimOffX = 0.0,  rdAimOffY = 0.0;

    // ── [2] CUE_MAX_POWER ────────────────────────────────────────────────
    static bool   bMaxPowerActive = false;
    static float  fMaxPowerMul    = 2.0f;
    static double origMaxPower    = 0.0;
    static double rdMaxPowerNow   = 0.0;

    // ── [3] CUE_SPIN ─────────────────────────────────────────────────────
    static bool   bMaxSpinActive = false;
    static float  fMaxSpinMul    = 2.0f;
    static double origMaxSpin    = 0.0;
    static double rdMaxSpinNow   = 0.0;

    // ── [4] VIP Hook ─────────────────────────────────────────────────────
    static bool bVIPActive = false;

    // ── [5] Pocket Nomination Bypass ─────────────────────────────────────
    static bool bNomBypass = false;

    // ── [6] Golden Shot Mode ─────────────────────────────────────────────
    static bool bGoldenShot = false;

    // ── [7] Packet Logger ────────────────────────────────────────────────
    static bool  bPacketLogActive     = false;
    static bool  bPacketHookInstalled = false;
    static std::atomic<int> iSendCount{0}, iRecvCount{0};
    static std::mutex  mtxPkt;
    static char  szLastSend[160] = "\xe2\x80\x94"; // "—"
    static char  szLastRecv[160] = "\xe2\x80\x94";
    static size_t lastSendLen = 0, lastRecvLen = 0;
    static ssize_t (*_origSend)(int, const void*, size_t, int) = nullptr;
    static ssize_t (*_origRecv)(int, void*, size_t, int)       = nullptr;

    // ── [8] API Probe ─────────────────────────────────────────────────────
    static std::atomic<bool> bAPITestRunning{false};
    static char szAPIResult[256] = "\xe2\x80\x94";

    // ── [9] Shot Power Force Max ──────────────────────────────────────────
    static bool   bShotPowerMax = false;
    static double rdPowerNow    = 0.0;

    // ── [10] Aim Angle Override ───────────────────────────────────────────
    static bool  bAimAngleOverride = false;
    static float fAimAngleDeg      = 0.0f;
    static double rdCurrentAngle   = 0.0;

    // ── [11] Enemy Ball Sink (ONE-SHOT) ───────────────────────────────────
    static char szSinkResult[64] = "\xe2\x80\x94";

    // ── [12] Zero Friction ────────────────────────────────────────────────
    static bool   bZeroFriction  = false;
    static bool   bFrictionPrev  = false;
    static bool   bFrictionSaved = false;
    static double origFriction[7] = {};

    // ── [13] Cue Ball Ghost ───────────────────────────────────────────────
    static bool   bCueBallGhost     = false;
    static bool   bCueBallGhostPrev = false;
    static float  fGhostRadius      = 0.25f;
    static double origCueBallRadius = 0.0;
    static double rdCueBallRadius   = 0.0;  // updated only when ghost active & in game

    // ── [14] Cue Ball Warp (ONE-SHOT) ─────────────────────────────────────
    static float fWarpX = 0.0f, fWarpY = 0.0f;

    // ── [15] Velocity Burst (ONE-SHOT) ────────────────────────────────────
    static float fBurstStrength = 80.0f;

    // ── [16] Ruleset Nomination Override ──────────────────────────────────
    static bool bRuleNomZero = false;

    // ── [17] Coins Write (ONE-SHOT) ───────────────────────────────────────
    static float fCoinsValue   = 9999999.0f;
    static char  szCoinsResult[64] = "\xe2\x80\x94";

    // ── [18] BACON Probe ──────────────────────────────────────────────────
    static std::atomic<bool> bBACONRunning{false};
    static char szBACONResult[256] = "\xe2\x80\x94";

    // ── [19] S3 Config Probe ──────────────────────────────────────────────
    static std::atomic<bool> bS3Running{false};
    static char szS3Result[256] = "\xe2\x80\x94";

    // ── [20] CCDirector Scan (READ-ONLY) ──────────────────────────────────
    static bool bDirScan       = false;
    static int  iDirScanOffset = 0x100;
    static char szDirScan[256] = "\xe2\x80\x94";

    // ════════════════════════════════════════════════════════════════════
    // UTILITY — semua cek SafeAddr dulu sebelum dereference
    // ════════════════════════════════════════════════════════════════════

    static inline bool SafeAddr(uintptr_t a) {
        // Reject null, kernel space, dan sangat besar (sign bit)
        return a > 0x10000UL && a < 0x0000800000000000ULL;
    }

    // mprotect → write → mprotect: untuk .rodata di libmain
    static bool SafeWriteDouble(uintptr_t addr, double val) {
        if (!SafeAddr(addr)) return false;
        long pgsz = sysconf(_SC_PAGESIZE);
        if (pgsz <= 0) pgsz = 4096;
        uintptr_t ps = addr & ~((uintptr_t)(pgsz - 1));
        if (mprotect((void*)ps, (size_t)(pgsz * 2), PROT_READ | PROT_WRITE) != 0)
            return false;
        *(double*)addr = val;
        mprotect((void*)ps, (size_t)(pgsz * 2), PROT_READ);
        return true;
    }

    // ─── Resolvers (safe: tidak crash kalau return 0) ────────────────────

    static uintptr_t GetGM() {
        if (!sharedGameManager || !SafeAddr(sharedGameManager.instance)) return 0;
        return sharedGameManager.instance;
    }

    static uintptr_t GetVCInst() {
        uintptr_t gm = GetGM(); if (!gm) return 0;
        uintptr_t vc = *(uintptr_t*)(gm + 0x4B8);
        return SafeAddr(vc) ? vc : 0;
    }

    static uintptr_t GetVGInst() {
        uintptr_t vc = GetVCInst(); if (!vc) return 0;
        uintptr_t vg = *(uintptr_t*)(vc + 0x3A8);
        return SafeAddr(vg) ? vg : 0;
    }

    static uintptr_t GetVECBase() {
        uintptr_t gm = GetGM(); if (!gm) return 0;
        uintptr_t vec = *(uintptr_t*)(gm + 0x4C8);
        return SafeAddr(vec) ? vec : 0;
    }

    static uintptr_t GetRuleset() {
        uintptr_t gm = GetGM(); if (!gm) return 0;
        uintptr_t rs = *(uintptr_t*)(gm + 0x3E0);
        return SafeAddr(rs) ? rs : 0;
    }

    static uintptr_t GetTableInst() {
        uintptr_t gm = GetGM(); if (!gm) return 0;
        uintptr_t tbl = *(uintptr_t*)(gm + 0x3E8);
        return SafeAddr(tbl) ? tbl : 0;
    }

    static FrictionProperties* GetFriction() {
        uintptr_t tbl = GetTableInst(); if (!tbl) return nullptr;
        uintptr_t fp  = *(uintptr_t*)(tbl + 0x3C0);
        return SafeAddr(fp) ? (FrictionProperties*)fp : nullptr;
    }

    struct BallData { uintptr_t inst; int cls; int state; };

    // Iterasi bola — HARUS dipanggil HANYA saat inGame==true
    static int GetAllBalls(BallData out[], int maxOut) {
        uintptr_t tbl = GetTableInst();
        if (!tbl) return 0;
        uintptr_t arrPtr = *(uintptr_t*)(tbl + 0x450);
        if (!SafeAddr(arrPtr)) return 0;
        uintptr_t count = *(uintptr_t*)(arrPtr + 0x08);
        if (count == 0 || count > 16) return 0;          // reject garbage
        uintptr_t* data = *(uintptr_t**)(arrPtr + 0x18);
        if (!SafeAddr((uintptr_t)data)) return 0;
        int found = 0;
        for (uintptr_t i = 0; i < count && found < maxOut; i++) {
            uintptr_t inst = data[i];
            if (!SafeAddr(inst)) continue;
            int cls   = *(int*)(inst + 0xA0);
            int state = *(int*)(inst + 0xA4);
            if (cls < -1 || cls > 10) continue;           // reject garbage
            out[found++] = { inst, cls, state };
        }
        return found;
    }

    // GetCueBall — HARUS dipanggil HANYA saat inGame==true
    static uintptr_t GetCueBall() {
        BallData b[16]; int n = GetAllBalls(b, 16);
        for (int i = 0; i < n; i++)
            if (b[i].cls == 0) return b[i].inst;
        return 0;
    }

    // ════════════════════════════════════════════════════════════════════
    //  TICK v4 — parameter inGame = g_espIsInGame dari DrawMenu
    //
    //  ZONA 1: libmain static constant — selalu aman, inGame tdk relevan
    //  ZONA 2: game object — hanya saat inGame==true, hanya saat perlu
    // ════════════════════════════════════════════════════════════════════
    static void Tick(bool inGame) {

        // ════ ZONA 1: libmain static — SELALU AMAN ════════════════════
        if (SafeAddr(libmain)) {
            uintptr_t addrP = libmain + 0x4E49410;
            uintptr_t addrS = libmain + 0x4E49418;

            // Live read
            rdMaxPowerNow = *(double*)addrP;
            rdMaxSpinNow  = *(double*)addrS;

            // [2] CUE_MAX_POWER write (.rodata, pakai mprotect)
            if (bMaxPowerActive) {
                if (origMaxPower == 0.0) origMaxPower = *(double*)addrP;
                SafeWriteDouble(addrP, origMaxPower * (double)fMaxPowerMul);
            } else if (origMaxPower != 0.0) {
                SafeWriteDouble(addrP, origMaxPower);
                origMaxPower = 0.0;
            }

            // [3] CUE_SPIN write
            if (bMaxSpinActive) {
                if (origMaxSpin == 0.0) origMaxSpin = *(double*)addrS;
                SafeWriteDouble(addrS, origMaxSpin * (double)fMaxSpinMul);
            } else if (origMaxSpin != 0.0) {
                SafeWriteDouble(addrS, origMaxSpin);
                origMaxSpin = 0.0;
            }
        }

        // ════ EARLY EXIT jika tidak in-game ═══════════════════════════
        // Semua kode di bawah ini memerlukan game object yang valid.
        // g_espIsInGame di-set oleh DrawESP setelah memverifikasi
        // mainStateManager.isInGame() == true.
        if (!inGame) {
            // Reset prev-flags supaya toggle terdeteksi saat masuk game
            // Jangan reset ke false — biarkan state tersimpan
            return;
        }

        // ════ ZONA 2: game object — inGame==true ══════════════════════

        // [1] mAimOffset — hanya akses VEC kalau experiment aktif
        if (bAimOffsetActive) {
            uintptr_t vec = GetVECBase();
            if (vec) {
                rdAimOffX = *(double*)(vec + 0x010);
                rdAimOffY = *(double*)(vec + 0x018);
                *(double*)(vec + 0x010) = (double)fAimOffsetX;
                *(double*)(vec + 0x018) = (double)fAimOffsetY;
            }
        }

        // [5] Pocket Nomination Bypass
        if (bNomBypass) {
            uintptr_t rs = GetRuleset();
            if (rs) *(int*)(rs + 0x068) = 0;
        }

        // [6] Golden Shot Mode
        if (bGoldenShot) {
            uintptr_t gm = GetGM();
            if (gm) *(int*)(gm + 0x5C0) = 16;
        }

        // [9] Shot Power Force Max — hanya akses VC kalau experiment aktif
        if (bShotPowerMax) {
            uintptr_t vc = GetVCInst();
            if (vc) {
                rdPowerNow = *(double*)(vc + 0x3B0);
                *(double*)(vc + 0x3B0) = 1.0;
            }
        }

        // [10] Aim Angle Override — hanya akses VG kalau experiment aktif
        if (bAimAngleOverride) {
            uintptr_t vg = GetVGInst();
            if (vg) {
                rdCurrentAngle = *(double*)(vg + 0x028);
                double rad = (double)fAimAngleDeg * (3.14159265358979 / 180.0);
                *(double*)(vg + 0x028) = rad;
            }
        }

        // [12] Zero Friction — tulis HANYA saat toggle berubah (saat in game)
        if (bZeroFriction != bFrictionPrev) {
            bFrictionPrev = bZeroFriction;
            FrictionProperties* fp = GetFriction();
            if (fp) {
                if (bZeroFriction) {
                    // Simpan original
                    origFriction[0] = fp->_coefficientOfSlidingFriction;
                    origFriction[1] = fp->_coefficientOfRollingFriction;
                    origFriction[2] = fp->_coefficientOfSpinningFriction;
                    origFriction[3] = fp->_timeOfequilibriumFactor;
                    origFriction[4] = fp->_velocityReductionSlidingFactor;
                    origFriction[5] = fp->_velocityReductionRollingFactor;
                    origFriction[6] = fp->_deltaSpinFactor;
                    bFrictionSaved  = true;
                    // Apply near-zero — lewati index 0 (offset 0x0) untuk aman
                    // kalau ternyata ada isa pointer di sana
                    fp->_coefficientOfRollingFriction    = 0.00015;
                    fp->_coefficientOfSpinningFriction   = 0.00015;
                    fp->_velocityReductionSlidingFactor  = 0.08;
                    fp->_velocityReductionRollingFactor  = 0.08;
                } else if (bFrictionSaved) {
                    fp->_coefficientOfSlidingFriction   = origFriction[0];
                    fp->_coefficientOfRollingFriction    = origFriction[1];
                    fp->_coefficientOfSpinningFriction   = origFriction[2];
                    fp->_timeOfequilibriumFactor         = origFriction[3];
                    fp->_velocityReductionSlidingFactor  = origFriction[4];
                    fp->_velocityReductionRollingFactor  = origFriction[5];
                    fp->_deltaSpinFactor                 = origFriction[6];
                    bFrictionSaved = false;
                }
            }
        }

        // [13] Cue Ball Ghost — tulis HANYA saat toggle berubah (saat in game)
        if (bCueBallGhost != bCueBallGhostPrev) {
            bCueBallGhostPrev = bCueBallGhost;
            uintptr_t cue = GetCueBall();   // AMAN: hanya di sini, in-game gate
            if (cue) {
                if (bCueBallGhost) {
                    origCueBallRadius = *(double*)(cue + 0x040);
                    *(double*)(cue + 0x040) = origCueBallRadius * (double)fGhostRadius;
                    rdCueBallRadius = *(double*)(cue + 0x040);
                } else if (origCueBallRadius != 0.0) {
                    *(double*)(cue + 0x040) = origCueBallRadius;
                    rdCueBallRadius   = origCueBallRadius;
                    origCueBallRadius = 0.0;
                }
            }
        }

        // [16] Ruleset Nomination Override
        if (bRuleNomZero) {
            uintptr_t rs = GetRuleset();
            if (rs) *(int*)(rs + 0x068) = 0;
        }

        // [20] CCDirector Scan — READ-ONLY, tidak ada write
        if (bDirScan && sharedDirector && SafeAddr(sharedDirector.instance)) {
            char tmp[256] = "";
            int found = 0;
            for (int off = iDirScanOffset;
                 off < iDirScanOffset + 0x100 && found < 6; off += 4) {
                uintptr_t addr = sharedDirector.instance + off;
                if (!SafeAddr(addr)) continue;
                float fv = *(float*)addr;
                if (fv > 0.01f && fv < 100.0f && !std::isnan(fv) && !std::isinf(fv)) {
                    char line[48];
                    snprintf(line, sizeof(line), "+%03X:%.3f  ", off, fv);
                    strncat(tmp, line, sizeof(tmp) - strlen(tmp) - 1);
                    found++;
                }
            }
            snprintf(szDirScan, sizeof(szDirScan), "DIR=%p\n%s",
                (void*)sharedDirector.instance,
                tmp[0] ? tmp : "(no float candidates)");
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // ONE-SHOT ACTIONS — dipanggil dari tombol, HARUS saat inGame==true
    // ════════════════════════════════════════════════════════════════════

    // [11] Enemy Ball Sink
    static void DoEnemySink() {
        BallData balls[16];
        int n = GetAllBalls(balls, 16);
        int cnt = 0;
        for (int i = 0; i < n; i++) {
            if (balls[i].cls == 0 || balls[i].cls == 4) continue; // skip cue & 8ball
            if (balls[i].state != 1 && balls[i].state != 2) continue;
            *(int*)(balls[i].inst + 0xA4) = 4;  // State::POTTED
            cnt++;
        }
        snprintf(szSinkResult, sizeof(szSinkResult),
            "Sink: %d balls potted", cnt);
    }

    // [14] Cue Ball Warp
    static void DoCueBallWarp() {
        uintptr_t cue = GetCueBall();
        if (!cue) { return; }
        *(double*)(cue + 0x020) = (double)fWarpX;
        *(double*)(cue + 0x028) = (double)fWarpY;
        *(double*)(cue + 0x030) = 0.0;   // zero velocity
        *(double*)(cue + 0x038) = 0.0;
    }

    // [15] Velocity Burst
    static void DoVelocityBurst() {
        BallData balls[16];
        int n = GetAllBalls(balls, 16);
        for (int i = 0; i < n; i++) {
            if (balls[i].cls == 0) continue;
            if (balls[i].state != 1 && balls[i].state != 2) continue;
            float ang = ((float)(rand() % 628)) / 100.0f;
            *(double*)(balls[i].inst + 0x030) = fBurstStrength * cosf(ang);
            *(double*)(balls[i].inst + 0x038) = fBurstStrength * sinf(ang);
        }
    }

    // [17] Coins Write
    static void DoCoinsWrite() {
        if (!sharedUserInfo || !SafeAddr(sharedUserInfo.instance)) {
            snprintf(szCoinsResult, sizeof(szCoinsResult), "ERR: no UserInfo");
            return;
        }
        uintptr_t cp = *(uintptr_t*)(sharedUserInfo.instance + 0x208);
        if (!SafeAddr(cp)) {
            snprintf(szCoinsResult, sizeof(szCoinsResult), "ERR: no coins ptr");
            return;
        }
        *(double*)(cp + 0x10) = (double)fCoinsValue;
        uintptr_t cashp = *(uintptr_t*)(sharedUserInfo.instance + 0x210);
        if (SafeAddr(cashp)) *(double*)(cashp + 0x10) = 9999.0;
        snprintf(szCoinsResult, sizeof(szCoinsResult),
            "Written %.0f", (double)fCoinsValue);
    }

    // ════════════════════════════════════════════════════════════════════
    // PACKET LOGGER
    // ════════════════════════════════════════════════════════════════════
    static void pktHex(const uint8_t* b, size_t len, char* out, size_t sz) {
        size_t n = len > 18 ? 18 : len, pos = 0;
        for (size_t i = 0; i < n && pos + 4 < sz; i++)
            pos += snprintf(out + pos, sz - pos, "%02X ", b[i]);
        if (len > 18 && pos + 3 < sz) snprintf(out + pos, sz - pos, "...");
    }

    static ssize_t my_send(int fd, const void* buf, size_t len, int flags) {
        ssize_t ret = _origSend(fd, buf, len, flags);
        if (bPacketLogActive && len >= 4 && len < 8192) {
            std::lock_guard<std::mutex> lk(mtxPkt);
            iSendCount.fetch_add(1);
            lastSendLen = len;
            pktHex((const uint8_t*)buf, len, szLastSend, sizeof(szLastSend));
        }
        return ret;
    }

    static ssize_t my_recv(int fd, void* buf, size_t len, int flags) {
        ssize_t ret = _origRecv(fd, buf, len, flags);
        if (bPacketLogActive && ret > 3 && (size_t)ret < 8192) {
            std::lock_guard<std::mutex> lk(mtxPkt);
            iRecvCount.fetch_add(1);
            lastRecvLen = (size_t)ret;
            pktHex((const uint8_t*)buf, (size_t)ret, szLastRecv, sizeof(szLastRecv));
        }
        return ret;
    }

    static void InstallPacketHooks() {
        if (bPacketHookInstalled) return;
        xhook_register(".*/com.miniclip.eightballpool/.*",
            "send", (void*)my_send, (void**)&_origSend);
        xhook_register(".*/com.miniclip.eightballpool/.*",
            "recv", (void*)my_recv, (void**)&_origRecv);
        xhook_refresh(0);
        bPacketHookInstalled = true;
    }

    static void SetPacketLog(bool on) {
        if (on && !bPacketHookInstalled) InstallPacketHooks();
        bPacketLogActive = on;
        if (!on) {
            iSendCount.store(0); iRecvCount.store(0);
            std::lock_guard<std::mutex> lk(mtxPkt);
            strcpy(szLastSend, "\xe2\x80\x94");
            strcpy(szLastRecv, "\xe2\x80\x94");
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // NETWORK PROBES
    // ════════════════════════════════════════════════════════════════════
    static void RunAPITest(const std::string& userId) {
        if (bAPITestRunning.load()) return;
        bAPITestRunning.store(true);
        snprintf(szAPIResult, sizeof(szAPIResult), "Probing...");
        std::thread([uid = userId]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                snprintf(szAPIResult, sizeof(szAPIResult), "FAIL: socket()");
                bAPITestRunning.store(false); return;
            }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "3.127.149.181", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                snprintf(szAPIResult, sizeof(szAPIResult), "REFUSED:80 (HTTPS-only)");
                close(sock); bAPITestRunning.store(false); return;
            }
            char req[400];
            snprintf(req, sizeof(req),
                "GET /v1/grant-reward?user_id=%s HTTP/1.1\r\n"
                "Host: prod-pool-reward-links-service.pool.miniclippt.com\r\n"
                "Connection: close\r\n\r\n", uid.c_str());
            send(sock, req, strlen(req), 0);
            char buf[512]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) {
                buf[n] = '\0';
                char* nl = strchr(buf, '\n'); if (nl) *nl = '\0';
                snprintf(szAPIResult, sizeof(szAPIResult), "%s", buf);
            } else {
                snprintf(szAPIResult, sizeof(szAPIResult), "No response");
            }
            bAPITestRunning.store(false);
        }).detach();
    }

    static void RunBACONProbe() {
        if (bBACONRunning.load()) return;
        bBACONRunning.store(true);
        snprintf(szBACONResult, sizeof(szBACONResult), "Probing...");
        std::thread([]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                snprintf(szBACONResult, sizeof(szBACONResult), "FAIL: socket");
                bBACONRunning.store(false); return;
            }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "54.73.12.200", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                sa.sin_port = htons(8080);
                if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                    snprintf(szBACONResult, sizeof(szBACONResult),
                        "REFUSED p80+p8080 (TLS only)");
                    close(sock); bBACONRunning.store(false); return;
                }
            }
            const char* wsReq =
                "GET /ws HTTP/1.1\r\n"
                "Host: pool-bacon-live.pool.miniclippt.com\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n";
            send(sock, wsReq, strlen(wsReq), 0);
            char buf[400]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) {
                buf[n] = '\0';
                char* nl = strchr(buf, '\n'); if (nl) *nl = '\0';
                snprintf(szBACONResult, sizeof(szBACONResult), "%s", buf);
            } else {
                snprintf(szBACONResult, sizeof(szBACONResult),
                    "Connected — no plaintext response");
            }
            bBACONRunning.store(false);
        }).detach();
    }

    static void RunS3Probe() {
        if (bS3Running.load()) return;
        bS3Running.store(true);
        snprintf(szS3Result, sizeof(szS3Result), "Probing...");
        std::thread([]() {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                snprintf(szS3Result, sizeof(szS3Result), "FAIL: socket");
                bS3Running.store(false); return;
            }
            struct timeval tv{6, 0};
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            struct sockaddr_in sa{}; sa.sin_family = AF_INET; sa.sin_port = htons(80);
            inet_pton(AF_INET, "52.217.10.100", &sa.sin_addr);
            if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
                snprintf(szS3Result, sizeof(szS3Result),
                    "TCP REFUSED:80 (HTTPS-only)");
                close(sock); bS3Running.store(false); return;
            }
            const char* req =
                "GET / HTTP/1.1\r\n"
                "Host: latest-live-config.pool.miniclippt.com\r\n"
                "Connection: close\r\n\r\n";
            send(sock, req, strlen(req), 0);
            char buf[400]{}; ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
            close(sock);
            if (n > 0) {
                buf[n] = '\0';
                char* nl = strchr(buf, '\n'); if (nl) *nl = '\0';
                snprintf(szS3Result, sizeof(szS3Result), "%s", buf);
            } else {
                snprintf(szS3Result, sizeof(szS3Result),
                    "No plaintext response (HTTPS-only)");
            }
            bS3Running.store(false);
        }).detach();
    }

    // ════════════════════════════════════════════════════════════════════
    // RESET ALL
    // ════════════════════════════════════════════════════════════════════
    static void ResetAll() {
        bAimOffsetActive  = false; fAimOffsetX = fAimOffsetY = 0.0f;
        bMaxPowerActive   = false; fMaxPowerMul = 2.0f;
        bMaxSpinActive    = false; fMaxSpinMul  = 2.0f;
        bVIPActive        = false;
        bNomBypass        = false;
        bGoldenShot       = false;
        SetPacketLog(false);
        bShotPowerMax      = false;
        bAimAngleOverride  = false;
        bZeroFriction      = false;  // Tick() akan restore di frame berikutnya (in game)
        bCueBallGhost      = false;  // Tick() akan restore di frame berikutnya (in game)
        bRuleNomZero       = false;
        bDirScan           = false;
        if (SafeAddr(libmain)) {
            if (origMaxPower != 0.0) {
                SafeWriteDouble(libmain + 0x4E49410, origMaxPower);
                origMaxPower = 0.0;
            }
            if (origMaxSpin != 0.0) {
                SafeWriteDouble(libmain + 0x4E49418, origMaxSpin);
                origMaxSpin = 0.0;
            }
        }
    }

} // namespace Experiment
