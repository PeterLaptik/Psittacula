#include "animation.h"
// ---------------------------------------------------------------------------
//  ParrotFlight
//  A flying yellow parrot drawn entirely with console characters.
//  The sprite is built from implicit shapes (ellipses + circle chains), so it
//  rescales smoothly to the current console window - resize it mid-flight
//  and the parrot grows/shrinks to fit.
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

using Clock = std::chrono::steady_clock;

static std::atomic<bool> is_working = false;

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

// -------------------------------------------------------------- Terminal --
// Flicker-free renderer: each frame we fill a CHAR_INFO grid in memory and
// hand the whole visible window to the console with one WriteConsoleOutput.
class Terminal {
    public:
    static constexpr WORD yellow = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_INTENSITY;
    static constexpr WORD dkYell = FOREGROUND_RED | FOREGROUND_GREEN;
    static constexpr WORD red = FOREGROUND_RED | FOREGROUND_INTENSITY;
    static constexpr WORD white = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    static constexpr WORD grey = FOREGROUND_RED | FOREGROUND_GREEN |
        FOREGROUND_BLUE;

    // Black sky: zero background bits on every cell.
    static constexpr WORD skyAttr = 0;

    explicit Terminal(HANDLE out) : out_(out) {
        CONSOLE_CURSOR_INFO ci{ sizeof(ci), 100 };
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

    void put(int x, int y, wchar_t ch, WORD attr) {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
        CHAR_INFO &c = grid_[static_cast<size_t>(y) * w_ + x];
        c.Char.UnicodeChar = ch;
        c.Attributes = attr;
    }

    void text(int x, int y, const std::wstring &s, WORD attr) {
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

    private:
    void showCursor() {
        CONSOLE_CURSOR_INFO ci{ sizeof(ci), 100 };
        ci.bVisible = TRUE;
        SetConsoleCursorInfo(out_, &ci);
    }

    HANDLE                    out_ = nullptr;
    std::unique_ptr<CHAR_INFO[]> grid_;
    short                     w_ = 0, h_ = 0;
    SHORT                     winLeft_ = 0, winTop_ = 0;
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

            wchar_t ch = 0;  WORD attr = 0;

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
    if (is_working.load() == true)
        return;

    is_working.store(true);

    Terminal term(GetStdHandle(STD_OUTPUT_HANDLE));
    if (!term.poll()) 
        return;

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
            Sleep(150);
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
        const double k = std::clamp(
            std::min((W - 2) / 13.0, (H - 3) / 12.5), 0.9, 6.0);

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
            const WORD attr = bob > 0 ? (Terminal::grey | FOREGROUND_INTENSITY)
                : Terminal::grey;
            for (int i = 0; i < sw; ++i) term.put(sx + i, liftRow, L'\x00b7', attr);
        }

        // status line
        term.text(1, H - 1, L"Checking connection...", Terminal::grey);

        term.present();
        Sleep(20);                        // ~50 fps

        if (!is_working.load())
            break;
    }
}



void stop_animation()
{
    is_working.store(false);
}
