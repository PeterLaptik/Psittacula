#include "animation.h"
// ---------------------------------------------------------------------------
//  ParrotFlight
//  A flying yellow parrot drawn entirely with console characters.
//  The sprite is built from implicit shapes (ellipses + circle chains), so it
//  rescales smoothly to the current console window - resize it mid-flight
//  and the parrot grows/shrinks to fit.
//
//  Platforms:
//    - Windows        console API backend (CONSOLE_SCREEN_BUFFER_INFO +
//                     one WriteConsoleOutputW blit per frame).
//    - FreeBSD etc.   POSIX tty backend: terminal size via
//                     ioctl(TIOCGWINSZ), rendering via VT100/ANSI escape
//                     sequences written straight to stdout. The cell grid
//                     is kept in memory and diffed against the previously
//                     shown frame; only changed cells are emitted, so the
//                     refresh does not flicker. Glyphs are encoded as
//                     UTF-8 unconditionally (no locale dependence).
// ---------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#endif

using Clock = std::chrono::steady_clock;

static std::atomic<bool> is_working(false);

#ifdef _WIN32
// ---------------------------------------------------------------------------
//  Windows backend
//  Flicker-free renderer: each frame we fill a CHAR_INFO grid in memory and
//  hand the whole visible window to the console with one WriteConsoleOutput.
// ---------------------------------------------------------------------------
class Terminal {
    public:
    using CellAttr = WORD;

    static constexpr CellAttr yellow = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_INTENSITY;
    static constexpr CellAttr dkYell = FOREGROUND_RED | FOREGROUND_GREEN;
    static constexpr CellAttr red = FOREGROUND_RED | FOREGROUND_INTENSITY;
    static constexpr CellAttr white = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    static constexpr CellAttr grey = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_BLUE;
    static constexpr CellAttr bright = FOREGROUND_INTENSITY;

    // Black sky: zero background bits on every cell.
    static constexpr CellAttr skyAttr = 0;

    explicit Terminal(HANDLE out) : out_(out) {
        CONSOLE_CURSOR_INFO ci{};
        ci.dwSize = 100;
        ci.bVisible = FALSE;
        SetConsoleCursorInfo(out_, &ci);
    }
    ~Terminal() { showCursor(); }

    // Track the visible window; (re)allocate the back buffer on change.
    bool poll() {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(out_, &info)) return false;
        const short w = static_cast<short>(info.srWindow.Right - info.srWindow.Left + 1);
        const short h = static_cast<short>(info.srWindow.Bottom - info.srWindow.Top + 1);
        if (w <= 0 || h <= 0) return false;
        // window origin can shift even when size is constant (scrolling) -
        // keep it fresh so our blit always lands on the visible area
        winLeft_ = info.srWindow.Left;
        winTop_ = info.srWindow.Top;
        if (!grid_ || w != w_ || h != h_) {
            w_ = w;  h_ = h;
            const size_t n = static_cast<size_t>(w_) * h_;
            grid_ = std::make_unique<CHAR_INFO[]>(n);
        }
        return true;
    }

    short width()  const { return w_; }
    short height() const { return h_; }

    void put(int x, int y, wchar_t ch, CellAttr attr) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        CHAR_INFO &c = grid_[static_cast<size_t>(y) * w_ + x];
        c.Char.UnicodeChar = ch;
        c.Attributes = attr;
    }

    void text(int x, int y, const std::wstring &s, CellAttr attr) {
        for (int i = 0; i < static_cast<int>(s.size()); ++i) put(x + i, y, s[i], attr);
    }

    // Fill the whole viewport with the sky dither (also our per-frame clear).
    void paintSky(unsigned frame) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                // sparse deterministic dither -> subtle atmosphere
                const uint32_t hsh =
                    (static_cast<uint32_t>(x) * 73856093u) ^
                    (static_cast<uint32_t>(y) * 19349663u) ^
                    frame;
                put(x, y, (hsh % 37u) == 0u ? L'\x00b7' : L' ',
                    skyAttr | grey);          // dim twinkling stars
            }
    }

    void present() {
        if (!grid_) return;
        SMALL_RECT region{
            winLeft_, winTop_,
            static_cast<SHORT>(winLeft_ + w_ - 1),
            static_cast<SHORT>(winTop_ + h_ - 1) };
        COORD size{ w_, h_ }, origin{ 0, 0 };
        WriteConsoleOutputW(out_, grid_.get(), size, origin, &region);
    }

    static void pauseMs(int ms) { ::Sleep(ms); }

    private:
    void showCursor() {
        CONSOLE_CURSOR_INFO ci{};
        ci.dwSize = 100;
        ci.bVisible = TRUE;
        SetConsoleCursorInfo(out_, &ci);
    }

    HANDLE                    out_ = nullptr;
    std::unique_ptr<CHAR_INFO[]> grid_;
    short                     w_ = 0, h_ = 0;
    SHORT                     winLeft_ = 0, winTop_ = 0;
};

#else
// ---------------------------------------------------------------------------
//  FreeBSD / POSIX backend
// ---------------------------------------------------------------------------

namespace {
    // write() loop: a tty write() may return short; EINTR is retried so a
    // stray signal (SIGWINCH included) cannot truncate a frame.
    bool writeFully(int fd, const char *data, size_t len) {
        size_t done = 0;
        while (done < len) {
            const ssize_t step = ::write(fd, data + done, len - done);
            if (step > 0) { done += static_cast<size_t>(step); continue; }
            if (step < 0 && errno == EINTR) continue;
            return false;
        }
        return true;
    }

    void appendNumber(std::string &buf, int value) {
        char tmp[12];
        int n = 0;
        if (value == 0) tmp[n++] = '0';
        while (value > 0) { tmp[n++] = static_cast<char>('0' + value % 10); value /= 10; }
        while (n > 0) buf += tmp[--n];
    }

    // Locale-independent wide -> UTF-8 converter. mbstate_t/wctomb follow the
    // process locale (the main app may have called setlocale with a
    // non-UTF-8 value), every modern FreeBSD terminal expects UTF-8 bytes -
    // encoding here is mandatory.
    void encodeUtf8(uint32_t cp, std::string &out) {
        if (cp < 0x80u) {                     // 1 byte
            out += static_cast<char>(cp);
        } else if (cp < 0x800u) {             // 2 bytes
            out += static_cast<char>(0xC0u | (cp >> 6));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else if (cp < 0x10000u) {           // 3 bytes - our glyphs stop here
            out += static_cast<char>(0xE0u | (cp >> 12));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        } else {                              // 4 bytes (defensive: unused)
            out += static_cast<char>(0xF0u | (cp >> 18));
            out += static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            out += static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            out += static_cast<char>(0x80u | (cp & 0x3Fu));
        }
    }
    using CellWord = std::uint16_t;   // packed glyph/attribute cell word
}  // namespace

// Diff renderer for VT/ANSI terminals: the scene is painted into a wcell_
// grid attribute-per-cell and present() pushes to the real terminal only the
// cells that changed since the previous frame. CellAttr encodes the classic
// 16-color text attribute: bits 0-2 hold the SGR foreground color
// (1 red, 2 green, 4 blue), bit 3 requests bright/intense rendering.
class Terminal {
    public:
    using CellAttr = CellWord;

    // Palette matching the Windows one; emitted as SGR 30-37 (optionally 1).
    static constexpr CellAttr bright = 8;
    static constexpr CellAttr yellow = 3 | bright;   // SGR 1;33 bright yellow
    static constexpr CellAttr dkYell = 3;            // SGR 33 dark yellow
    static constexpr CellAttr red    = 1 | bright;   // SGR 1;31 bright red
    static constexpr CellAttr white  = 7;            // SGR 37 white/grey
    static constexpr CellAttr grey   = 7;            // SGR 37 (rendered dim by
                                                     //  most palettes)

    // Black sky: attribute 0 on every cell (terminal default background).
    static constexpr CellAttr skyAttr = 0;

    Terminal() : tty_(::isatty(STDOUT_FILENO) != 0) {
        if (tty_) emit(kHideCursor);
    }
    ~Terminal() { if (tty_) emit(kShowCursor); }

    bool tty() const { return tty_; }

    // Poll terminal size; (re)allocate the back buffer on change.
    bool poll() {
        if (!tty_) return false;         // output is not a terminal
        winsize ws = {};
        if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) return false;
        const short w = static_cast<short>(ws.ws_col);
        const short h = static_cast<short>(ws.ws_row);
        if (w <= 0 || h <= 0) return false;
        if (!grid_ || w != w_ || h != h_) {
            w_ = w;  h_ = h;
            const size_t n = static_cast<size_t>(w_) * h_;
            grid_ = std::make_unique<WCell[]>(n);
            prev_ = std::make_unique<WCell[]>(n);
            // A resize re-lays out the screen in ways that cannot be modeled
            // here: clear it outright and force a full repaint of the new
            // viewport (prev_ starts zeroed, i.e. differs from every cell).
            emit("\x1b[m\x1b[2J");
        }
        return true;
    }

    short width()  const { return w_; }
    short height() const { return h_; }

    void put(int x, int y, wchar_t ch, CellAttr attr) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        WCell &c = grid_[static_cast<size_t>(y) * w_ + x];
        c.wc = ch;
        c.attr = attr;
    }

    void text(int x, int y, const std::wstring &s, CellAttr attr) {
        for (int i = 0; i < static_cast<int>(s.size()); ++i) put(x + i, y, s[i], attr);
    }

    // Fill the whole viewport with the sky dither (also our per-frame clear).
    void paintSky(unsigned frame) {
        for (int y = 0; y < h_; ++y)
            for (int x = 0; x < w_; ++x) {
                // sparse deterministic dither -> subtle atmosphere
                const uint32_t hsh =
                    (static_cast<uint32_t>(x) * 73856093u) ^
                    (static_cast<uint32_t>(y) * 19349663u) ^
                    frame;
                put(x, y, (hsh % 37u) == 0u ? L'\x00b7' : L' ',
                    skyAttr | grey);          // dim twinkling stars
            }
    }

    // Diff-push the grid to the terminal: only cells whose glyph or attribute
    // changed are emitted, as runs starting with an absolute cursor move and
    // an attribute set, so the update is invisible (no full-screen repaint).
    void present() {
        if (!grid_) return;

        std::string buf;
        buf.reserve(static_cast<size_t>(w_) * h_ * 3u + 64u);

        bool cursorValid = false;
        int cx = -1, cy = -1;
        CellAttr curAttr = static_cast<CellAttr>(0xFFFF);  // no attribute pending

        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                const size_t idx = static_cast<size_t>(y) * w_ + x;
                const WCell &c = grid_[idx];
                WCell &p = prev_[idx];

                if (c.wc == p.wc && c.attr == p.attr) continue;  // unchanged

                // The virtual cursor is not at this cell: reposition first.
                if (!cursorValid || cx != x || cy != y) {
                    buf += "\x1b[";
                    appendNumber(buf, y + 1);
                    buf += ';';
                    appendNumber(buf, x + 1);
                    buf += 'H';
                    cursorValid = true;
                }
                cx = x + 1;
                cy = y;
                if (cx >= w_) cx = -1;   // end of row: force reposition next

                if (c.attr != curAttr) {
                    if (c.attr == 0) {
                        buf += "\x1b[m";                    // default attribute
                    } else {
                        buf += "\x1b[";
                        buf += (c.attr & bright) ? "1;3" : "0;3";
                        buf += static_cast<char>('0' + (c.attr & 0x07));
                        buf += 'm';
                    }
                    curAttr = c.attr;
                }

                encodeUtf8(static_cast<uint32_t>(c.wc), buf);

                p = c;   // mark as already on screen
            }
        }

        if (!buf.empty())
            writeFully(STDOUT_FILENO, buf.data(), buf.size());
    }

    static void pauseMs(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    private:
    struct WCell {
        wchar_t wc = 0;
        CellAttr attr = 0;
    };

    static constexpr const char *kHideCursor = "\x1b[?25l";
    static constexpr const char *kShowCursor = "\x1b[?25h";

    void emit(const char *s) {
        if (s != nullptr)
            writeFully(STDOUT_FILENO, s, std::char_traits<char>::length(s));
    }

    bool tty_;
    std::unique_ptr<WCell[]> grid_, prev_;
    short w_ = 0, h_ = 0;
};

#endif  // _WIN32

// ---------------------------------------------------------------------------
//  Shared scene: uses the platform Terminal through the common API
//  (poll / width / height / put / text / paintSky / present / pauseMs /
//  CellAttr constants).
// ---------------------------------------------------------------------------

// -------------------------------------------------------------------- RNG --
struct Rng {                                    // xorshift32 - no <random>
    explicit Rng(uint32_t seed) : s_(seed ? seed : 1u) {}
    uint32_t next() {
        s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5;
        return s_;
    }
    int range(int lo, int hi) {                 // inclusive
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
    }
    private:
    uint32_t s_;
};

// ------------------------------------------------------------------ Art ---
struct Ellipse2 {  // axis-aligned ellipse in bird-local units (chars * k)
    double cx, cy, rx, ry;
    bool inside(double u, double v, double pad = 0.0) const {
        const double dx = (u - cx) / (rx + pad);
        const double dy = (v - cy) / (ry + pad);
        return dx * dx + dy * dy <= 1.0;
    }
};

struct Bird {
    double x = 0, y = 0;        // top-left of drawing area (chars)
    double k = 1.0;             // scale factor
    double flap = 0.0;          // wing phase
};

// Is (u,v) inside the wing (a chain of shrinking circles shoulder -> tip)?
static bool wingHit(double sx, double sy, double tx, double ty,
    double u, double v, double k) {
    for (int i = 0; i <= 5; ++i) {
        const double f = i / 5.0;
        const double px = sx + (tx - sx) * f;
        const double py = sy + (ty - sy) * f;
        const double pr = (0.45 + 0.35 * f) * k;
        const double dx = (u - px) / pr;
        const double dy = (v - py) / pr;
        if (dx * dx + dy * dy <= 1.0) return true;
    }
    return false;
}

// Parrot anatomy, all sizes in multiples of k (k = char scale):
//   tail   ellipse   head   beak   eye   body ellipse   flapping wing
static void drawBird(Terminal &t, const Bird &b) {
    const double k = b.k;
    const double bx = b.x, by = b.y;              // local origin

    // - shapes -------------------------------------------------------------
    const Ellipse2 body{ 6.0 * k, 4.6 * k, 3.1 * k, 3.3 * k };
    const Ellipse2 head{ 8.6 * k, 2.3 * k, 1.9 * k, 1.8 * k };
    const Ellipse2 tail{ 2.0 * k, 6.1 * k, 1.9 * k, 0.8 * k };
    const Ellipse2 beak{ 10.4 * k, 2.7 * k, 1.0 * k, 0.62 * k };
    const Ellipse2 eye{ 8.9 * k, 2.1 * k, 0.42 * k, 0.42 * k };

    // wing: chain of circles from shoulder to tip; tip angle sweeps with flap
    const double sx = 5.2 * k, sy = 4.0 * k;      // shoulder
    const double wingLen = 3.8 * k;
    const double ang = 2.9 + 0.8 * b.flap;        // radians, y grows downward
    const double tx = sx + std::cos(ang) * wingLen;
    const double ty = sy + std::sin(ang) * wingLen;

    const int aw = static_cast<int>(std::ceil(12.2 * k));  // drawing box
    const int ah = static_cast<int>(std::ceil(8.4 * k));
    const int x0 = static_cast<int>(std::floor(bx));
    const int y0 = static_cast<int>(std::floor(by));

    for (int r = y0; r < y0 + ah; ++r) {
        for (int c = x0; c < x0 + aw; ++c) {
            // sample at the cell centre, in bird-local units
            const double u = c - bx + 0.5;
            const double v = r - by + 0.5;

            wchar_t ch = 0;  Terminal::CellAttr attr = 0;

            if (eye.inside(u, v)) { ch = L'\x2588'; attr = Terminal::white; }
            else if (beak.inside(u, v)) { ch = L'\x2588'; attr = Terminal::red; }
            else if (wingHit(sx, sy, tx, ty, u, v, k)) {
                ch = L'\x2593'; attr = Terminal::dkYell;   // wing even over sky
            }
            else if (head.inside(u, v) || body.inside(u, v) || tail.inside(u, v)) {
                ch = L'\x2588'; attr = Terminal::yellow;
            }
            if (ch) t.put(c, r, ch, attr);
        }
    }

    // little feet under the body
    const int base = static_cast<int>(by + 7.4 * k);
    for (double fx : { 5.4, 6.6 }) {
        const int fxp = static_cast<int>(bx + fx * k);
        for (int dr = 0; dr < std::max(1, static_cast<int>(k * 0.9)); ++dr)
            t.put(fxp, base + dr, L'\x2588', Terminal::skyAttr | Terminal::grey);
    }
}

// --------------------------------------------------------------- Clouds ---
struct Cloud {
    int    x = 0, y = 0, len = 8;
    double acc = 0.0;           // fractional movement accumulator

    void respawn(Rng &rng, int W, int H) {
        len = rng.range(6, std::max(10, W / 6));
        y = rng.range(0, std::max(1, H / 2 - 1));
        x = W + rng.range(2, 40);
        acc = 0.0;
    }

    static void drift(std::vector<Cloud> &cs, double dt, int W, int H, Rng &rng) {
        for (Cloud &c : cs) {
            c.acc += dt * (4.0 + H / 14.0);   // chars per second
            while (c.acc >= 1.0) { c.x -= 1; c.acc -= 1.0; }
            if (c.x + c.len < 0) c.respawn(rng, W, H);
        }
    }
};

void start_animation()
{
    if (is_working.exchange(true))
        return;

    // Anything buffered in std::cout (e.g. the menu still on screen) must
    // not mix with the raw escape-byte stream we are about to write.
    std::cout.flush();

#ifdef _WIN32
    Terminal term(GetStdHandle(STD_OUTPUT_HANDLE));
#else
    Terminal term;
#endif
    if (!term.poll()) {
        is_working.store(false);    // no terminal: leave the flag clean
        return;
    }

    Rng rng(0xC0C02u);
    std::vector<Cloud> clouds(5);
    for (Cloud &c : clouds) c.respawn(rng, term.width(), term.height());

    auto last = Clock::now();
    double t = 0.0;                 // animation time (s)
    unsigned frame = 0;

    while (true)
    {
        if (!term.poll()) break;        // console went away / resized to 0
        const int W = term.width(), H = term.height();

        if (W < 24 || H < 10) {         // too small to fly: show a hint
            term.paintSky(frame);
            term.text(std::max(0, (W - 36) / 2), H / 2,
                L"Make the console window bigger!", Terminal::white);
            term.present();
            Terminal::pauseMs(150);
            continue;
        }

        // ---- timing --------------------------------------------------------
        const auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (dt > 0.25) dt = 0.25;       // don't jump after a stall
        t += dt;
        ++frame;

        // ---- simulation ------------------------------------------------------
        // scale: parrot full drawing is ~12k wide x ~9.5k tall (+feet/shadow)
        const double k = std::max(0.9,
            std::min(std::min((W - 2) / 13.0, (H - 3) / 12.5), 6.0));

        Bird bird;
        bird.k = k;
        bird.flap = std::sin(t * 7.0);                          // wing beat
        const double bob = std::sin(t * 2.2) * 1.3 * k;         // gentle rise/fall
        bird.x = (W - 12.0 * k) * (0.5 + 0.30 * std::sin(t * 0.27));
        bird.y = std::max(0.5, H * 0.16 + bob +
            std::sin(t * 0.45) * H * 0.08);

        Cloud::drift(clouds, dt, W, H, rng);

        // ---- draw ----------------------------------------------------------
        term.paintSky(frame);

        for (const Cloud &c : clouds)
            term.text(c.x, c.y, std::wstring(c.len, L'\x2593'), Terminal::grey);

        drawBird(term, bird);

        // soft ground shadow, fades as the bob lifts the bird
        {
            // the lower the bird (bigger bob), the wider and brighter the shadow
            const int liftRow = static_cast<int>(bird.y + 9.6 * k) + 1;
            const int sw = std::max(2, static_cast<int>(
                11.0 * k + bob * (4.0 / std::max(1.0, k))));
            const int sx = static_cast<int>(bird.x + 6.0 * k) - sw / 2;
            const Terminal::CellAttr attr = bob > 0 ? (Terminal::grey | Terminal::bright)
                : Terminal::grey;
            for (int i = 0; i < sw; ++i) term.put(sx + i, liftRow, L'\x00b7', attr);
        }

        // status line
        term.text(1, H - 1, L"Checking connection...", Terminal::grey);

        term.present();
        Terminal::pauseMs(20);            // ~50 fps

        if (!is_working.load())
            break;
    }
}

void stop_animation()
{
    is_working.store(false);
}
