#pragma once
// 养成逻辑独立于 Windows 窗口，可在任何平台直接编译测试。
// 数值规则与 2.1 保持一致（旧存档可无缝沿用），3.0 只新增统计字段。
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

struct PetLife {
    double satiety = 70, health = 95, mood = 80, energy = 85, grams = 450;
    int coins = 80, medicine = 1, vitamins = 0, recycled = 0, giftDate = 0;
    bool sick = false, toy = false, scarf = false, bed = false, bell = false, wearingScarf = false;
    double careCooldown = 0;
    // 3.0 新增统计：摸摸次数、相识日期（yyyymmdd）。
    int petCount = 0, firstDay = 0;

    void normalize() {
        auto bound = [](double value, double low, double high, double fallback) {
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        satiety = bound(satiety, 0, 100, 70); health = bound(health, 5, 100, 95);
        mood = bound(mood, 0, 100, 80); energy = bound(energy, 0, 100, 85);
        grams = bound(grams, 280, 800, 450);
        careCooldown = bound(careCooldown, 0, 300, 0);
        coins = std::clamp(coins, 0, 999999); medicine = std::clamp(medicine, 0, 99);
        vitamins = std::clamp(vitamins, 0, 99); recycled = std::clamp(recycled, 0, 999999);
        petCount = std::clamp(petCount, 0, 99999999);
        if (health < 45) sick = true;
        if (health >= 70) sick = false;
        if (!scarf) wearingScarf = false;
    }
    void advance(double seconds, bool sleeping, bool exercising) {
        if (!std::isfinite(seconds) || seconds <= 0) return;
        // 不补算关机、系统休眠或阻塞期间的流逝时间。
        seconds = std::min(seconds, 5.0);
        double minutes = seconds / 60;
        careCooldown = std::max(0.0, careCooldown - seconds);
        satiety -= minutes * (sleeping ? .14 : .25);
        energy += minutes * (sleeping ? (bed ? 1.5 : .9) : exercising ? -.7 : -.16);
        mood -= minutes * .055;
        if (satiety < 15) { health -= minutes * .8; grams -= minutes * .25; }
        else if (satiety > 92) { health -= minutes * .55; grams += minutes * .4; }
        else if (sleeping) health += minutes * (bed ? .65 : .35);
        else if (energy > 25) health += minutes * .04;
        if (energy < 12) health -= minutes * .3;
        if (grams < 340 || grams > 660) health -= minutes * .25;
        if (exercising && !sleeping) {
            grams -= minutes * (bell ? 1.4 : .8);
            mood += minutes * .8;
        }
        normalize();
    }
    void feedRecycled(int count) {
        if (count <= 0) return;
        count = std::min(count, 100);
        satiety += std::min(25, count * 6); grams += std::min(12, count * 2);
        mood += 6; coins += std::min(40, count * 3); recycled += count;
        normalize();
    }
    void pet(double moodGain = 2) { mood += moodGain; ++petCount; normalize(); }
    double bodyScale() const { return std::clamp(1.0 + (grams - 450) / 800, .82, 1.27); }
    const wchar_t* shape() const { return grams < 380 ? L"偏瘦" : grams > 580 ? L"偏胖" : L"匀称"; }
    const wchar_t* status() const {
        if (sick) return L"需要照护";
        if (satiety < 20) return L"肚子饿了";
        if (energy < 18) return L"有点犯困";
        if (health < 70) return L"有些疲惫";
        if (mood < 30) return L"想要陪伴";
        return L"状态良好";
    }
    static int price(int item) { constexpr int prices[] = {25,18,60,90,80,60}; return item >= 0 && item < 6 ? prices[item] : 999999; }
    bool owned(int item) const { return item == 2 ? toy : item == 3 ? scarf : item == 4 ? bed : item == 5 ? bell : false; }
    bool buy(int item) {
        if (item < 0 || item >= 6 || owned(item) || coins < price(item)) return false;
        if ((item == 0 && medicine >= 99) || (item == 1 && vitamins >= 99)) return false;
        coins -= price(item);
        switch (item) {
        case 0: ++medicine; break; case 1: ++vitamins; break; case 2: toy = true; break;
        case 3: scarf = true; wearingScarf = true; break; case 4: bed = true; break; case 5: bell = true; break;
        }
        normalize(); return true;
    }
    bool takeMedicine() { if (!medicine) return false; --medicine; health += 35; energy += 8; normalize(); return true; }
    bool takeVitamins() { if (!vitamins) return false; --vitamins; health += 12; energy += 20; normalize(); return true; }
    bool care() { if (careCooldown > 0) return false; careCooldown = 300; health += 8; mood += 10; normalize(); return true; }
    bool dailyGift(int date) { if (date <= giftDate) return false; giftDate = date; coins += 20; normalize(); return true; }
    // 运动只在健康、精力允许时开始；进行中精力过低或生病会自动结束。
    bool canExercise() const { return !sick && energy >= 20; }
    bool mustStopExercise() const { return sick || energy < 10; }
};

// yyyymmdd → 自公元纪年起的天数（公历，适用于计算相伴天数）。
inline long civilDays(int yyyymmdd) {
    int y = yyyymmdd / 10000, m = yyyymmdd / 100 % 100, d = yyyymmdd % 100;
    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe;
}
inline int daysTogether(int firstDay, int today) {
    long a = civilDays(firstDay), b = civilDays(today);
    if (!a || !b || b < a) return 1;
    return static_cast<int>(b - a) + 1;
}

// 可移植的路径层级判断，输入须已由 Windows 规范化为绝对路径。
inline std::wstring pathKey(std::wstring value) {
    for (auto& c : value) { if (c == L'/') c = L'\\'; if (c >= L'A' && c <= L'Z') c += L'a' - L'A'; }
    while (value.size() > 3 && value.back() == L'\\') value.pop_back();
    return value;
}
inline bool sameOrInside(const std::wstring& path, const std::wstring& directory) {
    const auto p = pathKey(path), d = pathKey(directory);
    if (p.empty() || d.empty()) return false;
    return p == d || (p.size() > d.size() && p.compare(0, d.size(), d) == 0 && (d.back() == L'\\' || p[d.size()] == L'\\'));
}
// TSF_DELETE_RECYCLE_IF_POSSIBLE：只有带此标志的删除才是“移入回收站”。
inline bool mayRecycleFromFlags(unsigned flags) { return (flags & 0x80u) != 0; }
