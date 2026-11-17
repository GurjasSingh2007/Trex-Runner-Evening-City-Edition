#pragma once
#include <vector>
#include <random>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "assets.h"

using namespace std;

namespace trex {

const int W = 600, H = 200;
enum State { END = 0, PLAY = 1 };

const double START_SPEED  = 6.0;
const double SPEED_STEP   = 1.0;
const double MAX_SPEED    = 30.0;
const double JUMP_SPEED   = 13.0;
const int    LEVEL_SCORE  = 150;
const double PLANE_CHANCE  = 0.30;
const double TRUCK_CHANCE = 0.40;
const double GAP_MIN_FRAMES = 45, GAP_EXTRA_FRAMES = 40;
const int    HARD_START_SCORE = 500, HARD_FULL_SCORE = 2000;
const double HARD_GAP_LESS = 20, HARD_EXTRA_LESS = 20;
const double HARD_PLANE_BONUS = 0.20;
const double GROUND_TOP = 185;
const double ROAD_TOP = 176;
const double VEHICLE_BOTTOM = 186;
const double PLANE_Y[3] = { 160, 136, 90 };

struct Sprite {
    double x = 0, y = 0, vx = 0, vy = 0;
    double cw = 0, ch = 0;
    const Image* img = nullptr;
    const Image* alt = nullptr;
    int tick = 0;
    int life = -1;
    bool dead = false;
};

inline const Image* currentImage(const Sprite& s) { return (s.alt && (s.tick / 8) % 2) ? s.alt : s.img; }

inline void placement(const Image& im, double cx, double cy, int& left, int& top) {
    left = (int)floor(cx - im.ow / 2 + 0.5);
    top  = (int)floor(cy - im.oh / 2 + 0.5);
}

inline bool overlap(const Sprite& a, const Sprite& b) {
    return a.x - a.cw / 2 < b.x + b.cw / 2 && a.x + a.cw / 2 > b.x - b.cw / 2 &&
           a.y - a.ch / 2 < b.y + b.ch / 2 && a.y + a.ch / 2 > b.y - b.ch / 2;
}

inline bool pixelsTouch(const Image& ia, double ax, double ay, const Image& ib, double bx, double by) {
    int al, at, bl, bt; placement(ia, ax, ay, al, at); placement(ib, bx, by, bl, bt);
    int x0 = max(al, bl), x1 = min(al + ia.w, bl + ib.w);
    int y0 = max(at, bt), y1 = min(at + ia.h, bt + ib.h);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (ia.mask[(y - at) * ia.w + (x - al)] && ib.mask[(y - bt) * ib.w + (x - bl)]) return true;
    return false;
}

struct Input {
    bool space = false, down = false;
    bool restartKey = false;
    bool click = false;
    int mx = 0, my = 0;
};

class Game {
public:
    State state = PLAY;
    int score = 0, level = 1;
    double speed = START_SPEED;
    long frameCount = 0;
    int highScore = 0;
    int popupTicks = 0;
    bool recordDirty = false;
    void setHighScore(int v) { highScore = v < 0 ? 0 : v; }
    bool takeRecordDirty() { bool d = recordDirty; recordDirty = false; return d; }

    Game() : rng(random_device{}()) { buildBackground(); setup(); }

    void setup() {
        trex = Sprite(); trex.x = 50; trex.cw = 89 * 0.5; trex.ch = 94 * 0.5;
        trex.y = GROUND_TOP - trex.ch / 2;
        ground = Sprite(); ground.x = IMG_ground.w / 2.0; ground.y = ROAD_TOP + IMG_ground.h / 2.0; ground.img = &IMG_ground;
        gameOver = Sprite(); gameOver.x = 300; gameOver.y = 70; gameOver.img = &IMG_gameOver;
        restart = Sprite();  restart.x = 300;  restart.y = 110; restart.img = &IMG_restart;
        restart.cw = IMG_restart.ow; restart.ch = IMG_restart.oh;
        invisibleGround = Sprite(); invisibleGround.x = 200; invisibleGround.y = GROUND_TOP + 5;
        invisibleGround.cw = 400; invisibleGround.ch = 10;
        reset();
    }

    void step(const Input& in) {
        frameCount++;
        if (state == PLAY) {
            score += 1;
            if (highScore > 0 && !crossed && score > highScore) { crossed = true; popupTicks = 150; }
            if (score >= LEVEL_SCORE * level) { speed = min(MAX_SPEED, speed + SPEED_STEP); level++; }
            ground.vx = -speed;
            farOff  = fmod(farOff  + speed * 0.10, W);
            nearOff = fmod(nearOff + speed * 0.30, W);
            for (auto& o : obstacles) o.vx = -speed - (o.alt ? 1.0 : 0.0);

            bool grounded = trex.y + trex.ch / 2 >= 182.5;
            if (in.space && grounded) trex.vy = -JUMP_SPEED;
            setDucking(in.down && grounded && !in.space);
            trex.vy += 0.8;

            if (ground.x < 0) ground.x += IMG_ground.w / 2.0;
            if (overlap(trex, invisibleGround)) {
                trex.y = invisibleGround.y - invisibleGround.ch / 2 - trex.ch / 2;
                trex.vy = 0;
            }
            spawnClouds();
            spawnObstacles();

            const Image& ti = trexImage();
            for (auto& o : obstacles)
                if (overlap(o, trex) && pixelsTouch(ti, trex.x, trex.y, *currentImage(o), o.x, o.y)) {
                    state = END; endTicks = 0; setDucking(false);
                    if (score > highScore) { highScore = score; recordDirty = true; }
                    break;
                }
            for (auto& o : obstacles) o.tick++;
        } else {
            ground.vx = 0; trex.vy = 0; endTicks++;
            for (auto& o : obstacles) { o.vx = 0; o.life = -1; }
            for (auto& c : clouds)    { c.vx = 0; c.life = -1; }
            bool overRestart = in.click &&
                fabs(in.mx - restart.x) < restart.cw / 2 && fabs(in.my - restart.y) < restart.ch / 2;
            bool spacePress = in.space && !prevSpace && endTicks > 30;
            if (overRestart || in.restartKey || spacePress) reset();
        }
        prevSpace = in.space;
        if (popupTicks > 0) popupTicks--;
        updateAll();
        if (state == PLAY) animTick++;
    }

    double difficulty() const { return min(1.0, max(0.0, (score - HARD_START_SCORE) / double(HARD_FULL_SCORE - HARD_START_SCORE))); }

    void render(uint32_t* fb) const {
        memcpy(fb, bg.data(), sizeof(uint32_t) * W * H);
        for (auto& c : clouds) drawSprite(fb, c);
        int top = (int)ROAD_TOP + 4;
        for (int i = 0; i < 2; i++) {
            blit(fb, IMG_far,  -(int)farOff  + i * W, top - IMG_far.h);
            blit(fb, IMG_near, -(int)nearOff + i * W, top - IMG_near.h);
        }
        drawSprite(fb, ground);
        drawImage(fb, trexImage(), trex.x, trex.y);
        if (state == END) { drawSprite(fb, gameOver); drawSprite(fb, restart); }
        for (auto& o : obstacles) drawImage(fb, *currentImage(o), o.x, o.y);
    }

private:
    Sprite trex, ground, gameOver, restart, invisibleGround;
    vector<Sprite> clouds, obstacles;
    vector<uint32_t> bg;
    int animTick = 0, endTicks = 0;
    bool ducking = false, prevSpace = false, crossed = false;
    double farOff = 0, nearOff = 0;
    mt19937 rng;

    double rnd(double a, double b) { return uniform_real_distribution<double>(a, b)(rng); }

    void buildBackground() {
        static const double st[4][4] = { {0, 0x22, 0x1e, 0x57}, {.45, 0x6a, 0x3f, 0x86}, {.75, 0xe8, 0x71, 0x8a}, {1, 0xf9, 0xa6, 0x5a} };
        bg.assign(W * H, 0xFF000000u);
        for (int y = 0; y < H; y++) {
            double t = min(1.0, y / ROAD_TOP); int i = 0; while (i < 2 && t > st[i + 1][0]) i++;
            double f = (t - st[i][0]) / (st[i + 1][0] - st[i][0]); unsigned c[3];
            for (int k = 0; k < 3; k++) c[k] = (unsigned)(st[i][k + 1] + (st[i + 1][k + 1] - st[i][k + 1]) * f);
            fill(bg.begin() + y * W, bg.begin() + (y + 1) * W, 0xFF000000u | c[0] << 16 | c[1] << 8 | c[2]);
        }
        static const int stars[][2] = { {60,18},{150,34},{215,14},{290,46},{350,23},{410,12},{465,33},{565,17},{520,52},{100,60} };
        for (auto& s : stars) bg[s[1] * W + s[0]] = 0xFFFFF4F8u;
    }

    const Image& trexImage() const {
        if (state == END) return IMG_trex_collided;
        if (ducking) return (animTick / 6) % 2 ? IMG_duck2 : IMG_duck1;
        static const Image* f[3] = { &IMG_trex1, &IMG_trex3, &IMG_trex4 };
        return *f[(animTick / 4) % 3];
    }

    void setDucking(bool d) {
        if (d == ducking) return;
        double bottom = trex.y + trex.ch / 2;
        ducking = d;
        if (d) { trex.cw = IMG_duck1.ow; trex.ch = IMG_duck1.oh; } else { trex.cw = 89 * 0.5; trex.ch = 94 * 0.5; }
        trex.y = bottom - trex.ch / 2;
    }

    void spawnClouds() {
        if (frameCount % 90 != 0) return;
        Sprite c; c.x = W + 30; c.y = floor(rnd(15, 95) + 0.5);
        c.img = &IMG_cloud; c.vx = -1.5; c.life = 450;
        clouds.push_back(c);
    }

    void spawnObstacles() {
        if (animTick < 50) return;
        double lastRight = -1e9;
        for (auto& o : obstacles) lastRight = max(lastRight, o.x + o.cw / 2);
        if (lastRight > W - nextGap) return;
        Sprite o; o.life = 600;
        double d = difficulty();
        if (rnd(0, 1) < PLANE_CHANCE + HARD_PLANE_BONUS * d) {
            o.img = &IMG_plane1; o.alt = &IMG_plane2; o.y = PLANE_Y[min(2, (int)rnd(0, 3))];
        } else {
            static const Image* cars[2] = { &IMG_car_red, &IMG_car_blue };
            static const Image* trucks[2] = { &IMG_truck_a, &IMG_truck_b };
            o.img = (rnd(0, 1) < TRUCK_CHANCE ? trucks : cars)[rnd(0, 1) < .5 ? 0 : 1];
            o.y = VEHICLE_BOTTOM - o.img->oh / 2;
        }
        o.cw = o.img->ow; o.ch = o.img->oh; o.x = W + o.cw / 2 + 2;
        o.vx = -speed - (o.alt ? 1.0 : 0.0);
        nextGap = (GAP_MIN_FRAMES - HARD_GAP_LESS * d + rnd(0, GAP_EXTRA_FRAMES - HARD_EXTRA_LESS * d)) * (speed + 1);
        obstacles.push_back(o);
    }
    double nextGap = 0;

    void reset() {
        obstacles.clear(); clouds.clear();
        score = 0; level = 1; speed = START_SPEED; nextGap = 0; crossed = false; popupTicks = 0;
        ground.vx = -speed; animTick = 0; state = PLAY; ducking = false;
    }

    static void updateSprite(Sprite& s) { s.x += s.vx; s.y += s.vy; if (s.life > 0 && --s.life == 0) s.dead = true; }
    void updateAll() {
        updateSprite(trex); updateSprite(ground);
        for (auto& c : clouds) updateSprite(c);
        for (auto& o : obstacles) updateSprite(o);
        auto gone = [](const Sprite& s) { return s.dead || s.x < -150; };
        clouds.erase(remove_if(clouds.begin(), clouds.end(), gone), clouds.end());
        obstacles.erase(remove_if(obstacles.begin(), obstacles.end(), gone), obstacles.end());
    }

    void drawSprite(uint32_t* fb, const Sprite& s) const { if (s.img) drawImage(fb, *s.img, s.x, s.y); }
    static void drawImage(uint32_t* fb, const Image& im, double cx, double cy) {
        int l, t; placement(im, cx, cy, l, t); blit(fb, im, l, t);
    }
    static void blit(uint32_t* fb, const Image& im, int left, int top) {
        int j0 = max(0, -top), j1 = min(im.h, H - top);
        int i0 = max(0, -left), i1 = min(im.w, W - left);
        for (int j = j0; j < j1; j++) {
            const unsigned char* p = im.rgba + (j * im.w + i0) * 4; uint32_t* d = fb + (top + j) * W + left + i0;
            for (int i = i0; i < i1; i++, p += 4, d++) {
                unsigned a = p[3]; if (!a) continue;
                if (a == 255) { *d = 0xFF000000u | p[0] << 16 | p[1] << 8 | p[2]; continue; }
                unsigned dr = (*d >> 16) & 255, dg = (*d >> 8) & 255, db = *d & 255;
                *d = 0xFF000000u | ((p[0] * a + dr * (255 - a)) / 255) << 16 | ((p[1] * a + dg * (255 - a)) / 255) << 8 | ((p[2] * a + db * (255 - a)) / 255);
            }
        }
    }
};

}
