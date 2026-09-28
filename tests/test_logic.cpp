// 养成逻辑测试：2.1 的全部断言保留，并补充 3.0 新增规则。
#include "../src/pet_logic.h"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    PetLife p;
    p.feedRecycled(0); assert(p.coins == 80 && p.satiety == 70);
    p.feedRecycled(2); assert(p.coins == 86 && p.satiety == 82 && p.grams == 454 && p.recycled == 2);
    p.feedRecycled(1000000); assert(p.coins == 126 && p.satiety == 100 && p.recycled == 102);
    p.coins = 24; assert(!p.buy(0) && p.coins == 24 && p.medicine == 1);
    p.coins = 200; assert(p.buy(3) && p.wearingScarf && p.coins == 110);
    assert(!p.buy(3) && p.coins == 110);
    assert(!p.buy(-1) && !p.buy(6));
    p.health = 40; p.normalize(); assert(p.sick);
    assert(p.takeMedicine() && p.health == 75 && !p.sick);
    assert(!p.takeMedicine() && p.health == 75);
    assert(p.care()); double h = p.health; assert(!p.care() && p.health == h);
    assert(p.dailyGift(20260928)); int coins = p.coins;
    assert(!p.dailyGift(20260928) && !p.dailyGift(20260927) && p.coins == coins);
    p.grams = 650; double fat = p.bodyScale(); p.grams = 330; assert(p.bodyScale() < fat);
    p.grams = 650; p.satiety = 65; p.energy = 90;
    for (int i = 0; i < 720; ++i) p.advance(5, false, true);
    assert(p.grams < 650 && p.satiety < 65 && p.energy < 90);
    double energy = p.energy; for (int i=0;i<60;++i) p.advance(5,true,false);
    assert(p.energy > energy);
    p.satiety = 60; p.advance(86400, false, false); assert(p.satiety > 59);
    p.health = std::numeric_limits<double>::quiet_NaN(); p.normalize(); assert(std::isfinite(p.health));
    assert(sameOrInside(L"C:\\Users\\A\\Desktop\\a.txt", L"c:\\users\\a\\desktop"));
    assert(!sameOrInside(L"C:\\WindowsOld\\a.txt", L"C:\\Windows"));
    assert(sameOrInside(L"C:\\folder\\sub", L"C:\\folder\\"));
    assert(sameOrInside(L"C:\\anything", L"C:\\"));
    assert(!sameOrInside(L"",L"C:\\") && !sameOrInside(L"C:\\a",L""));
    assert(!mayRecycleFromFlags(0) && mayRecycleFromFlags(0x80) && mayRecycleFromFlags(0x180));
    std::cout << "PASS: economy, health, recovery, weight, time caps, rewards, path boundaries and recycle-only guard\n";

    // 3.0 新增
    PetLife q; q.mood = 99; q.pet(); assert(q.mood == 100 && q.petCount == 1);
    q.pet(); assert(q.petCount == 2 && q.mood == 100);
    q.careCooldown = 1e9; q.normalize(); assert(q.careCooldown == 300);
    q.careCooldown = std::numeric_limits<double>::infinity(); q.normalize(); assert(q.careCooldown == 0);
    q.energy = 19; assert(!q.canExercise()); q.energy = 20; assert(q.canExercise());
    q.energy = 9; assert(q.mustStopExercise()); q.energy = 50; q.sick = true; assert(q.mustStopExercise() && !q.canExercise());
    assert(daysTogether(20260928, 20260928) == 1);
    assert(daysTogether(20260101, 20260928) == 271);
    assert(daysTogether(20241231, 20250101) == 2);
    assert(daysTogether(20240228, 20240301) == 3); // 闰年
    assert(daysTogether(0, 20260928) == 1 && daysTogether(20270101, 20260928) == 1);
    PetLife s; s.satiety = 10; assert(std::wstring(s.status()) == L"肚子饿了");
    s.satiety = 60; s.health = 40; s.normalize(); assert(std::wstring(s.status()) == L"需要照护");
    std::cout << "PASS: petting stats, cooldown bounds, exercise guards, days together, status text\n";
}
