#define _CRT_SECURE_NO_WARNINGS
// 禁用 Microsoft Visual C++ 编译器对“不安全函数”（如 strcpy, scanf）的警告 [6]

#include <easyx.h>
// 引入 EasyX 图形库头文件，用于绘图、窗口创建和图像加载 [5]

#include <vector>
// 标准模板库中的动态数组容器，用于存储怪物、宝箱等对象列表

#include <ctime>
// 提供 time() 函数，用于获取当前时间作为随机种子

#include <cstdlib>
// 包含 rand() 和 srand() 函数，用于生成伪随机数

#include <tchar.h>
// 支持 Unicode 或多字节字符集的宏定义，兼容 _T("") 宏 [3]

#pragma comment(lib,"Winmm.lib")
// 链接 Windows 多媒体库，使 MessageBox 等 API 可用 [6]

/* ---------- 常量 ---------- */
const int TILE = 32;            // 每个地图格子的像素尺寸为 32x32
const int COLS = 25;            // 地图总列数（水平方向）
const int ROWS = 19;            // 地图总行数（垂直方向）
const int WIDTH = COLS * TILE;  // 游戏窗口宽度（像素）
const int HEIGHT = (ROWS + 1) * TILE; // 窗口高度：+1 行用于顶部状态栏显示信息

/* ---------- 贴图 ---------- */
IMAGE* imgFloor, * imgWall, * imgChest, * imgMonster, * imgGoal, * imgPerson;
// 定义六个 IMAGE 类型指针，分别指向地板、墙、宝箱、怪物、终点、玩家的图像资源

/* ---------- 结构 ---------- */
struct Pos { int x, y; };
// 位置结构体，表示二维坐标点 (x,y)，用于描述宝箱、怪物、玩家的位置

struct Monster {
    Pos p;                      // 怪物当前位置
    int hp, atk;                // 生命值和攻击力
    int moveCD = 0;             // 移动冷却计时器，控制其移动频率
};
// 定义怪物结构体，包含位置、属性和行为控制字段

// 新增：伤害飘字结构体（扩展功能）
struct DamageText {
    Pos p;                      // 显示位置
    int hp;                     // 显示的伤害数值
    int life;                   // 存活帧数，用于逐渐消失效果
};
// 用于实现“-20”这类数字从怪物头上飘起的效果 [7]

/* ---------- 全局对象 ---------- */
std::vector<Monster> monsters;  // 存储所有活跃怪物的动态列表
std::vector<Pos> treasure;      // 存储所有掉落宝箱的位置
std::vector<std::vector<int>> mapv;
// 二维整型向量表示地图数据：0=通路，1=墙
int px = 0, py = 0;             // 玩家初始坐标（左上角起点）
int playerHP = 100, playerATK = 20;
// 玩家初始生命值与攻击力
int chestTotal = 0, chestGot = 0;
// 记录游戏中应拾取的总宝箱数及已拾取数量

// 新增打击效果相关全局变量（扩展功能）
int shakeDuration = 0;           // 屏幕震动持续帧数
int shakeOffsetX = 0, shakeOffsetY = 0;
// 当前屏幕偏移量，用于实现震动动画
std::vector<DamageText> damageTexts;
// 存储正在显示的伤害飘字效果

/* ---------- 工具 ---------- */
void loadTile(IMAGE*& img, const TCHAR* file) {
    img = new IMAGE;             // 动态分配内存给图像对象
    if (loadimage(img, file, TILE, TILE, true) != 0) {
        // 尝试加载指定路径的图片，并缩放至 32x32 像素；若失败返回非零值
        MessageBox(nullptr, file, _T("缺失"), MB_OK);
        // 弹出对话框提示哪个文件未找到 [3][4]
        exit(1);                 // 加载失败则终止程序
    }
}
// 此函数统一加载游戏所需的所有贴图资源

bool canMove(int x, int y) {
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return false;
    // 判断是否超出地图边界
    return mapv[y][x] == 0;      // 返回该位置是否为空地（非墙）
}
// 用于判断玩家或怪物能否移动到目标格子

/* ---------- 绘制 ---------- */
void draw() {
    // ★ 更新震动效果：每帧减少持续时间并随机偏移 
    if (shakeDuration > 0) {
        if (--shakeDuration == 0) {
            shakeOffsetX = 0;
            shakeOffsetY = 0;
            // 震动结束，恢复原位
        }
        else {
            shakeOffsetX = rand() % 6 - 3; // ±3像素抖动
            shakeOffsetY = rand() % 6 - 3;
            // 随机产生轻微偏移，模拟屏幕晃动
        }
    }

    BeginBatchDraw();            // 开始批量绘制，减少画面闪烁
    setfillcolor(BLACK);         // 设置填充颜色为黑色
    solidrectangle(0, 0, WIDTH - 1, HEIGHT - 1);
    // 绘制黑色背景矩形，覆盖整个窗口

    /* 地图 */
    for (int i = 0; i < ROWS; ++i)
        for (int j = 0; j < COLS; ++j) {
            // ★ 加上震动偏移绘制地板和墙
            putimage(j * TILE + shakeOffsetX, (i + 1) * TILE + shakeOffsetY, imgFloor);
            // 绘制地板图块，加上震动偏移
            if (mapv[i][j])
                putimage(j * TILE + shakeOffsetX, (i + 1) * TILE + shakeOffsetY, imgWall);
            // 若是墙，则叠加绘制墙体图块
        }

    // Lambda 函数：带透明混合模式绘制精灵
    auto putT = [](int x, int y, IMAGE* img) {
        putimage(x * TILE + shakeOffsetX, (y + 1) * TILE + shakeOffsetY, img, SRCAND);
        // 使用 AND 模式去除黑底
        putimage(x * TILE + shakeOffsetX, (y + 1) * TILE + shakeOffsetY, img, SRCPAINT);
        // 使用 PAINT 模式叠加图像主体
        };

    for (auto& m : monsters) putT(m.p.x, m.p.y, imgMonster);
    // 绘制所有怪物
    for (auto& t : treasure) putT(t.x, t.y, imgChest);
    // 绘制所有宝箱
    putT(COLS - 1, ROWS - 1, imgGoal);
    // 绘制终点标志
    putT(px, py, imgPerson);
    // 绘制玩家角色

    /* 状态栏 */
    setfillcolor(RGB(255, 255, 255));
    // 设置白色填充色
    solidrectangle(10, 5, 400, 30);
    // 绘制状态栏背景矩形
    TCHAR buf[128];
    _stprintf(buf, _T("HP:%d  ATK:%d  还需拾取:%d"), playerHP, playerATK, chestTotal - chestGot);
    // 格式化状态文本：显示当前血量、攻击、剩余宝箱数
    setbkmode(TRANSPARENT);
    // 文字背景设为透明
    settextcolor(BLACK);
    // 字体颜色设为黑色
    outtextxy(15, 10, buf);
    // 在指定位置输出状态信息

    /* ---------- 绘制伤害数字 ---------- */
    settextcolor(RED);
    // 设置伤害文字为红色
    setbkmode(TRANSPARENT);
    // 背景透明
    for (auto& dt : damageTexts) {
        int screenX = dt.p.x * TILE + shakeOffsetX + TILE / 2 - 10;
        // 计算 X 坐标：居中偏移
        int screenY = (dt.p.y + 1) * TILE + shakeOffsetY - dt.life;
        // Y 坐标随 life 递减而上升（实现向上飘动）
        _stprintf(buf, _T("-%d"), dt.hp);
        // 格式化为“-XX”形式
        outtextxy(screenX, screenY, buf);
        // 输出伤害数字
    }

    FlushBatchDraw();
    // 提交所有绘制命令到屏幕，完成刷新
}

/* ---------- 战斗 ---------- */
void tryBattle() {
    for (auto it = monsters.begin(); it != monsters.end(); ++it) {
        int dx = abs(it->p.x - px), dy = abs(it->p.y - py);
        // 计算怪物与玩家之间的曼哈顿距离分量
        if (dx + dy <= 2) {   // 攻击范围两格内 

            // ★ 添加：生成伤害数字
            damageTexts.push_back({ it->p, playerATK, 20 });
            // 创建一个新伤害飘字，显示玩家造成的伤害值

            // ★ 添加：击退效果（沿远离玩家方向推一格）
            int pushX = it->p.x + (it->p.x > px ? 1 : -1);
            int pushY = it->p.y + (it->p.y > py ? 1 : -1);
            // 计算远离玩家的方向
            if (canMove(pushX, it->p.y)) it->p.x = pushX;
            // 优先尝试水平方向击退
            else if (canMove(it->p.x, pushY)) it->p.y = pushY;
            // 否则尝试垂直方向

            // 扣血
            it->hp -= playerATK;

            // ★ 添加：屏幕震动 + 短暂停顿增强打击感 
            shakeDuration = 5;
            // 触发短时间震动反馈
            Sleep(40); // Hit Stop：攻击命中瞬间停顿
            // 暂停 40ms 模拟“命中重击”的节奏感 [7]

            if (it->hp <= 0) {
                treasure.push_back({ it->p.x, it->p.y });
                // 掉落宝箱
                chestTotal++;
                // 总宝箱数增加
                monsters.erase(it);
                // 从列表中移除已死亡怪物

                // 死亡时更明显反馈
                Sleep(60);
                // 更长的停顿强化击杀体验
                return;
            }

            playerHP -= it->atk;
            // 怪物反击，玩家扣血
            if (playerHP <= 0) return;
            // 若玩家死亡则不再处理后续逻辑
            break;
            // 每次只攻击一个怪物
        }
    }
}

/* ---------- 输入 ---------- */
bool keyPressed[256] = {};
// 记录每个按键是否正处于“已按下但未释放”状态，防止重复触发移动

void movePlayer() {
    bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
    bool j = (GetAsyncKeyState('J') & 0x8000) != 0;
    // 获取 WASD 和 J 键的异步按键状态（是否被按下）

    int nx = px, ny = py;
    // 初始化新坐标为当前坐标

    if (w && !keyPressed['W']) { --ny; keyPressed['W'] = true; }
    if (s && !keyPressed['S']) { ++ny; keyPressed['S'] = true; }
    if (a && !keyPressed['A']) { --nx; keyPressed['A'] = true; }
    if (d && !keyPressed['D']) { ++nx; keyPressed['D'] = true; }
    if (j && !keyPressed['J']) { keyPressed['J'] = true; tryBattle(); }
    // 如果键首次被按下（未锁定），则执行一次动作并标记为“已按”

    if (!w) keyPressed['W'] = false;
    if (!s) keyPressed['S'] = false;
    if (!a) keyPressed['A'] = false;
    if (!d) keyPressed['D'] = false;
    if (!j) keyPressed['J'] = false;
    // 如果键已松开，则解除锁定，允许下次再次触发

    if (nx == px && ny == py) return;
    // 无有效移动请求则直接返回
    if (!canMove(nx, ny)) return;
    // 新位置不可通行也返回

    px = nx; py = ny;
    // 更新玩家坐标

    // 拾取宝箱
    for (auto it = treasure.begin(); it != treasure.end(); ++it)
        if (it->x == px && it->y == py) {
            treasure.erase(it);
            chestGot++;
            break;
        }
    // 遍历宝箱列表，若玩家到达其位置则拾取

    /* 碰到怪即死 */
    for (auto& m : monsters)
        if (m.p.x == px && m.p.y == py) {
            playerHP = 0;
            return;
        }
    // 若玩家与任意怪物同格，则立即死亡
}

/* ---------- 怪物移动 ---------- */
void moveMonsters() {
    for (auto& m : monsters) {
        if (--m.moveCD > 0) continue;
        // 冷却未结束则跳过本次移动

        int dx = px - m.p.x;
        int dy = py - m.p.y;
        // 计算朝向玩家的差值
        if (dx == 0 && dy == 0) continue;
        // 若已重合则不移动

        int nx = m.p.x + (dx > 0 ? 1 : dx < 0 ? -1 : 0);
        int ny = m.p.y + (dy > 0 ? 1 : dy < 0 ? -1 : 0);
        // 计算下一步趋向玩家的位置（每次仅移动一格）

        if (canMove(nx, ny)) { m.p.x = nx; m.p.y = ny; }
        // 若目标位置合法，则更新坐标

        m.moveCD = 20;   // 减速至约 0.7 秒/格
        // 设置较长的移动冷却时间，显著降低怪物速度 [5]
    }
}

/* ---------- 主函数 ---------- */
int main() {
    initgraph(WIDTH, HEIGHT);
    // 初始化图形窗口，大小为 WIDTH × HEIGHT

    loadTile(imgFloor, _T("./assets/floor.png"));
    loadTile(imgWall, _T("./assets/wall.png"));
    loadTile(imgChest, _T("./assets/chest.png"));
    loadTile(imgMonster, _T("./assets/monster.png"));
    loadTile(imgGoal, _T("./assets/goal.png"));
    loadTile(imgPerson, _T("./assets/person.png"));
    // 加载六种贴图资源，失败会弹窗退出 [3]

    /* 随机地图 */
    mapv.assign(ROWS, std::vector<int>(COLS, 0));
    // 初始化二维地图为全 0（空地）
    srand((unsigned)time(nullptr));
    // 使用当前时间为种子初始化随机数生成器
    for (int i = 0; i < ROWS; ++i)
        for (int j = 0; j < COLS; ++j)
            if (rand() % 10 < 3) mapv[i][j] = 1;
    // 以 30% 概率设置墙壁
    mapv[0][0] = 0;
    mapv[ROWS - 1][COLS - 1] = 0;
    // 强制起点和终点为空地

    /* 随机怪物 & 宝箱 */
    int mobCnt = 5, chestCnt = 3;
    for (int c = 0; c < mobCnt; ) {
        int x = rand() % COLS, y = rand() % ROWS;
        if (mapv[y][x] == 0 && !(x == 0 && y == 0)) {
            bool ok = true;
            for (auto& m : monsters) if (m.p.x == x && m.p.y == y) ok = false;
            // 检查是否已有怪物在此位置
            if (ok) { monsters.push_back({ {x, y}, 50, 15 }); ++c; }
            // 添加新怪物：位置、生命值 50、攻击力 15
        }
    }

    for (int c = 0; c < chestCnt; ) {
        int x = rand() % COLS, y = rand() % ROWS;
        if (mapv[y][x] == 0 && !(x == 0 && y == 0)) {
            bool ok = true;
            for (auto& m : monsters) if (m.p.x == x && m.p.y == y) ok = false;
            for (auto& t : treasure)   if (t.x == x && t.y == y) ok = false;
            // 确保不与怪物或已有宝箱重叠
            if (ok) { treasure.push_back({ x, y }); ++c; }
            // 添加宝箱
        }
    }
    chestTotal = (int)treasure.size();
    chestGot = 0;
    // 初始化宝箱统计

    /* 开局提示 */
    MessageBox(nullptr,
        _T("规则：\n1. WASD移动，J攻击（两格内有效）。\n")
        _T("2. 击败怪物掉落宝箱，拾齐后走到终点通关。\n")
        _T("3. 碰到怪即死，HP≤0失败。"),
        _T("游戏说明"), MB_OK | MB_ICONINFORMATION);
    // 显示游戏规则说明对话框 [3]

    /* 主循环 */
    while (true) {
        movePlayer();
        moveMonsters();
        draw();

        // 更新伤害数字生命值
        for (auto it = damageTexts.begin(); it != damageTexts.end(); ) {
            if (--it->life <= 0) it = damageTexts.erase(it);
            else ++it;
        }
        // 每帧减少 life，归零后从列表中删除

        if (playerHP <= 0) {
            MessageBox(nullptr, _T("游戏结束：玩家阵亡！"), _T("提示"), MB_OK);
            break;
        }
        if (px == COLS - 1 && py == ROWS - 1) {
            if (chestGot < chestTotal)
                MessageBox(nullptr, _T("请先拾取所有宝箱！"), _T("提示"), MB_OK);
            else {
                MessageBox(nullptr, _T("恭喜通关！"), _T("胜利"), MB_OK);
                break;
            }
        }
        Sleep(16);
        // 控制帧率约为 60 FPS（16ms/帧）[5]
    }

    closegraph();
    // 关闭图形窗口
    return 0;
    // 程序正常退出
}