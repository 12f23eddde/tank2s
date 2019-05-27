#define _CRT_SECURE_NO_WARNINGS 1
//#define _BOTZONE_ONLINE

//Tank2S minMax version
//Modified from 1-level evaluation
#include<vector>
#include<queue>
#include<map>
#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include "jsoncpp/json.h"
#include<algorithm>
#include<cmath>

/**调试用控制变量**/
bool print_distance = false;
#ifndef _BOTZONE_ONLINE
bool manual_input = true;
#define DEBUG
#endif
/**调试用控制变量**/
using namespace std;
const int dx[4] = { 0,1,0,-1 };
const int dy[4] = { -1,0,1,0 };
const int none = 0, brick = 1, steel = 2, water = 4, forest = 8, base = 16;
const int blue = 0, red = 1;
const int INF = 10000000;

inline bool is_move(int action) { return action >= 0 && action <= 3; }
inline bool is_shoot(int action) { return action >= 4; }
inline int opposite_shoot(int act) { return act > 5 ? act - 2 : act + 2; }
inline int abs(int a) { return a > 0 ? a : -a; }
inline int min(int a, int b) { return a < b ? a : b; }

// 全局
int self, enemy;
bool vague_death = false;

///cmath里面竟然没有符号函数
inline int sgn(double x){
    if(x>0) return 1;
    if(x<0) return -1;
    return 0;
}
inline bool inMap(int x, int y){
    return x >= 0 && x <= 8 && y >= 0 && y <= 8;
}
struct Pos {
	int x, y;
};


struct Tank {
	int x, y;
	int action = -1;///前一回合的行动
	int steps_in_forest = 0;
	bool side;
	bool dead = false;
	bool killed = false;///在上一回合被杀死
	int n_fire_rounds = 1;
	inline void setpos(int _x, int _y) {
		if (x == -2) steps_in_forest++;
		else steps_in_forest = 0;
		x = _x, y = _y;
	}
	inline void setact(int act) { action = act; }
	inline void die() { x = -1; setact(-1); dead = true;}
	inline bool alive() const { return x != -1; }
	inline bool can_shoot()const {
		return n_fire_rounds > 0;
	}
	inline bool in_forest() { return x == -2; }///判断是否在树林里 只对对方
	inline bool at(int _x, int _y)const { return x == _x && y == _y; }
	int & operator[](int index) { if (index == 0)return x; else return y; }
	void move(int act) {
		if (act == -1)return;
		x += dx[act];
		y += dy[act];
		action = act;
        n_fire_rounds++;
	}
	void shoot(int act) { action = act; n_fire_rounds = 0;}
	bool operator==(const Tank & t)const { return x == t.x&&y == t.y; }
};

struct Action{
    int a[2] = {0};
    double ev = -INF; //估值
    int & operator [] (int k){
        return a[k];
    }
};

struct Battlefield {

	int state[9][9];///地图
	static bool possible_field[2][9][9];///possible enemy position in forest
	int distance[2][9][9];
	bool route_searched[2] = {false, false};
	int rounds = 0;///回合数
    double value = 0;///局面估值, 暂时废弃
	static int stay;///暂时废弃，没有太大用处
	Tank tank[2][2];
    void print_state(){
        ///用于调试输出地图
#ifdef DEBUG
        for (int y = 0; y < 9; y++)
        {
            for (int x = 0; x < 9; x++)
            {
                if(y == tank[0][0].y and x == tank[0][0].x or y == tank[0][1].y and x == tank[0][1].x){
                    cout << "B";
                    continue;
                }
                if(y == tank[1][0].y and x == tank[1][0].x or y == tank[1][1].y and x == tank[1][1].x){
                    cout << "R";
                    continue;
                }
                switch (state[y][x])
                {
                    case none:
                        cout << ' ';
                        break;
                    case brick:
                        cout << '#';
                        break;
                    case base:
                        cout << 'X';
                        break;
                    case steel:
                        cout << '%';
                        break;
                    case forest:
                        cout << '.';
                        break;
                    case water:
                        cout << 'W';
                        break;
                }
            }
            cout << endl;
        }
        cout << endl;
#endif
    }
     void print_possible(){
#ifdef DEBUG
        for(int i=0;i<2;i++){
            cout << "enemy id: " << i << endl;
            for(int j=0;j<9;j++){
                for(int k=0;k<9;k++){
                    if(possible_field[i][j][k])
                        cout << "X";
                    else cout << "O";
                }
                cout << endl;
            }
        }
#endif
    }
	void init_tank() {
		tank[blue][0].setpos(2, 0);
		tank[blue][1].setpos(6, 0);
		tank[red][0].setpos(6, 8);
		tank[red][1].setpos(2, 8);
        tank[blue][0].side = tank[blue][1].side = blue;
        tank[red][0].side = tank[red][1].side = red;
	}

	Battlefield & operator=(const Battlefield & b) {
		memcpy(state, b.state, sizeof(state));
		memcpy(tank, b.tank, sizeof(tank));
		rounds = b.rounds;
		return *this;
	};

	Battlefield(const Battlefield &b) {
		*this = b;
	}

	~Battlefield() { }

	bool operator<(const Battlefield & b) { return value < b.value; }

	void init(Json::Value & info) {
		memset(state, 0, 81 * sizeof(int));
		memset(possible_field, false, sizeof(possible_field));
		state[0][4] = state[8][4] = base;
		Json::Value requests = info["requests"], responses = info["responses"];
		//assert(requests.size());
		//load battlefield info
		self = requests[0u]["mySide"].asInt();
		enemy = 1 - self;
		init_tank();
		for (unsigned j = 0; j < 3; j++)
		{
			int x = requests[0u]["brickfield"][j].asInt();
			for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * brick;
		}
		for (unsigned j = 0; j < 3; j++)
		{
			int x = requests[0u]["forestfield"][j].asInt();
			for (int k = 0; k < 27; k++) {
				state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * forest;
			}
		}
		for (unsigned j = 0; j < 3; j++)
		{
			int x = requests[0u]["steelfield"][j].asInt();
			for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * steel;
		}
		for (unsigned j = 0; j < 3; j++)
		{
			int x = requests[0u]["waterfield"][j].asInt();
			for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * water;
		}
	}


	Battlefield() {	}
    /**基础功能相关函数**/
	bool tankOK(int x, int y)const// can a tank step in?
	{
		return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (state[y][x] == none || state[y][x] == forest);
	}

	bool lose(bool enemy = false) {
		if (state[0][4] == none && state[8][4] == none) {
			return false;
		}
		int nowside = self;
		if (enemy)nowside = 1 - nowside;
		if (state[0][4] == none && nowside == 0) return true;
		if (state[8][4] == none && nowside == 1) return true;
		return false;
	}

    int getGameState() { // -1 仍在继续 0 蓝胜 1 红胜 2 平局
        bool blue_lost = false, red_lost = false;
        if(tank[blue][0].dead&&tank[blue][1].dead) blue_lost = true;
        if(state[0][4] == none) blue_lost = true;
        if(tank[red][0].dead&&tank[red][1].dead) red_lost = true;
        if(state[8][4] == none) red_lost = true;
        if(blue_lost){
            if(red_lost) return 2;
            else return red;
        } else if(red_lost) return blue;
        else return -1;
    }
	///判断两辆坦克是否相对
    ///坦克的战略纵深 越深的话潜在距离越短
    int depth(Tank & tmp){
        int delta_y = 0;
        delta_y = tmp.side ? -1 : 1;
        int x = tmp.x, y = tmp.y + delta_y;
        int strategy_depth = 0;
        while(inMap(x, y)){
            ///note: 以后加forest
            if(state[y][x] == 0){
                strategy_depth++;
                y += delta_y;
            }
            else break;
        }
        return strategy_depth;
    }

    bool isOpposite(Tank & tank1, Tank & tank2)
    {
        if(tank1.dead or tank2.dead) return false;
        if (tank1.x != tank2.x and tank1.y != tank2.y)//不在同一直线上
            return false;
        if (tank1.x == tank2.x)
        {
            int x = tank1.x;
            int y1 = tank1.y, y2 = tank2.y;
            if (y1 == y2) return false;//在同一个格子上应该不算陷入江局
            if (y1 > y2)
            {
                int temp; temp = y1, y1 = y2, y2 = temp;
            }

            for (int i = y1 + 1; i < y2; i++)
            {
                if ((state[i][x] & forest) or (state[i][x] & water) or (state[i][x] == 0))
                {
                    continue;
                }
                else//中间有遮挡则不算对面
                    return false;
            }
            return true;
        }
        else if (tank1.y == tank2.y)
        {
            //大规模代码复用
            int y = tank1.y;
            int x1 = tank1.x, x2 = tank2.x;
            if (x1 == x2) return false;
            if (x1 > x2)
            {
                int temp; temp = x1, x1 = x2, x2 = temp;
            }

            for (int i = x1 + 1; i < x2; i++)
            {
                if ((state[y][i] & forest) or (state[y][i] & water) or (state[y][i] == 0))
                {
                    continue;
                }
                else
                    return false;
            }
            return true;
        }
        else return false;
    }

	// 判断当前位置是否会被攻击
	bool in_danger(int side, int id)const {
		if (tank[side][id].dead) return false;
		if (!tank[side][0].can_shoot() && !tank[side][1].can_shoot())return false;
		if (can_attack_base(side, id))return false;
		for (size_t i = 0; i < 2; i++)
		{
			if (tank[1 - side][i].x == tank[side][id].x && tank[1 - side][i].y == tank[self][id].y)
				return false;
			if (tank[1 - side][i].alive() && tank[1 - side][i].can_shoot() &&
				can_shoot(*this, tank[1 - side][i].x, tank[i - side][i].y, tank[side][id].x, tank[side][id].y))
				return true;
		}
		return false;
	}

    ///敌人是否可能在弹道上 ，或者弹道旁边
	bool possible_enemy(int side, int x, int y, int dir)const {
		for (size_t j = 0; j < 2; j++)
		{
			if (tank[1 - side][j].x == x && tank[1 - side][j].y == y)return true;
			if (side == self && possible_field[j][y][x] == true)return true;
		}
		int neibor[4] = { x + dy[dir],y + dx[dir],x - dy[dir],y - dx[dir] };
		for (size_t i = 0; i < 2; i++)
		{
			if (!tankOK(neibor[i * 2], neibor[i * 2 + 1]))continue;
			for (size_t j = 0; j < 2; j++)
			{
				if (!tank[1 - side][j].alive())continue;
				if (tank[1 - side][j].x == neibor[i * 2] && tank[1 - side][j].y == neibor[i * 2 + 1])return true;
				if (side == self && possible_field[j][neibor[i * 2 + 1]][neibor[i * 2]] == true)return true;
			}
		}
		return false;
	}


	// 判断坐标x，y是不是自己射的
	// 如果是，那么清空该路径上所有的可能森林
	bool shoot_by_self(int x, int y) {
#ifdef DEBUG
	    cout << "[shoot by self check]: position " << y << ' ' << x << endl;
#endif
	    bool self_shoot[2] = {0,0};///0：不是自己打的 1：自己一辆坦克打的 2：自己两辆坦克都可能打了
		for (size_t i = 0; i < 2; i++)
		{
			if (!tank[self][i].alive())continue;
			if (x == tank[self][i].x && y == tank[self][i].y)continue;
			int act = tank[self][i].action;
#ifdef DEBUG
			cout << "act: " << act << endl;
#endif
			if (act > 3) {
				if(act % 2 == 0 and x != tank[self][i].x) continue;
				if(act % 2 == 1 and y != tank[self][i].y) continue;
				int xx = tank[self][i].x + dx[act - 4], yy = tank[self][i].y + dy[act - 4];
				cout << yy << ' ' << xx << endl;
				while(inMap(xx,yy)){
				    if(xx == x and yy == y){
				        self_shoot[i] = true;
				        break;
				    }
				    if(state[yy][xx] == steel or state[yy][xx] == brick or state[yy][xx] == base) break;
				    if(yy == tank[self][1-i].y and xx == tank[self][1-i].x) break;
				    if((yy == tank[enemy][0].y and xx == tank[enemy][0].x) or (yy == tank[enemy][1].y and xx == tank[enemy][1].x)) break;
				    xx += dx[act - 4], yy += dy[act - 4];
				}
 			}
		}
		///两辆坦克都没有打
        if(!self_shoot[0] and !self_shoot[1]) return false;
        if(self_shoot[0] and self_shoot[1]){
            ///这种情况下有可能是都打中了，也有可能是草丛对射
            ///因此保险起见不清理possible field
            return true;
        }
        else{
            cout << "oneshot" << endl;
            int id;
            if(self_shoot[0]) id = 0;
            else id = 1;
            int act = tank[self][id].action;
            int xx = tank[self][id].x + dx[act - 4], yy = tank[self][id].y + dy[act - 4];
            ///清理possible field
            while(inMap(xx,yy)){
                if(xx == x and yy == y) break;
                possible_field[0][yy][xx] = possible_field[1][yy][xx] = false;
                xx += dx[act - 4], yy += dy[act - 4];
            }
            print_possible();
        }
        return true;
	}
	///通过敌方摧毁物体推测树林中敌人的位置
	bool set_forest_shooter(int x, int y) {
	    cout << "[debug]: " << y << ' ' << x << endl;
		bool tmp_field[2][9][9];
		bool possible[2] = {false, false};///极端情况下，某个方块可能是被两辆坦克射的
		bool clear = false;///是否能够弄清到底是哪一个坦克
		bool dr[4] = {0,0,0,0};///四个方向的可能性
		memset(tmp_field,0, sizeof(tmp_field));
		for(int i=0;i<4;i++){
		    int xx = x + dx[i], yy = y + dy[i];
		    while(inMap(xx,yy)){
                if(state[yy][xx] == brick or state[yy][xx] == steel or state[yy][xx] == base) break;
                if(state[yy][xx] == none or state[yy][xx] == water){
                    xx += dx[i], yy += dy[i];
                    continue;
                }
                for(int j=0;j<2;j++){
                    if(possible_field[j][yy][xx] and tank[enemy][j].can_shoot()){
                        possible[j] = true;
                        tmp_field[j][yy][xx] = true;
                    }
                }
                xx += dx[i], yy += dy[i];
		    }
		}
		if(possible[0]!=possible[1]){///只有其中一辆坦克有可能射击到
		    if(possible[0]){
                memcpy(possible_field[0], tmp_field[0], sizeof(possible_field[0]));
                tank[enemy][0].n_fire_rounds = 0;
                tank[enemy][0].action = 6;///这个数随便写，没有影响，只是标注一下它开炮了
                tank[enemy][0].steps_in_forest = 0;
		    }
		    else{
                memcpy(possible_field[1], tmp_field[1], sizeof(possible_field[1]));
                tank[enemy][1].n_fire_rounds = 0;
                tank[enemy][1].action = 6;
                tank[enemy][1].steps_in_forest = 0;
		    }
		    clear = true;
		}
		return clear;
	}
	///假如本来能够打中方块而没有打中，路线上必定存在坦克
	///写了一半我发现这东西涉及到上一回合状态，只能放到update函数里面了，开头一段结尾一段
	/*bool shoot_not_work(Tank & tank){
	    int act = tank.action;
	    if(act < 4) return false;///虽然这个函数不是让你这么用的但还是加了判断
	    int x = tank.x, y = tank.y;
	    int will_destroy[2] = {-1, -1};///即将摧毁的方块坐标
	    int xx = x + dx[act - 4], yy = y + dy[act - 4];
	    while(inMap(xx,yy)){
	        if(state[yy][xx] == base or state[yy][xx] == brick){
	            will_destroy[0] = yy, will_destroy[1] = xx;
	            break;
	        }
	        else if(state[yy][xx] == none or state[yy][xx] == forest or state[yy][xx] == water){
	            xx += dx[act - 4], yy += dy[act - 4];
                continue;
	        }
	        else break;
	    }
	    if(state)
	}*/
	// quick_play
	// 不检查合法性
	//不能处理草丛敌人，使用前需要先locate enemy
	void quick_play(int * actionlist) {
		tank[self][0].action = actionlist[0];
		tank[self][1].action = actionlist[1];
		tank[enemy][0].action = actionlist[2];
		tank[enemy][1].action = actionlist[3];
		///处理坦克移动
		for(size_t tmpside = 0;tmpside<2;tmpside++){
		    for(size_t tmp_id = 0; tmp_id < 0; tmp_id++){
		        if(tank[tmpside][tmp_id].dead)
                    tank[tmpside][tmp_id].killed = false;
		        ///如果一开始就死了，那必定不是被杀的
		    }
		}
		for (size_t id = 0; id < 2; id++)
		{
			if (!tank[self][id].alive())continue;
			if (actionlist[id] >= 4)continue;
			tank[self][id].move(actionlist[id]);
		}
		for (size_t id = 0; id < 2; id++)
		{
			if (!tank[enemy][id].alive())continue;
			if (actionlist[id + 2] >= 0 && actionlist[id + 2] < 4) {
				tank[enemy][id].move(actionlist[id + 2]);
			}
		}
		///处理摧毁物体以及死亡坦克
		vector<Pos> destroylist;
		///处理敌方坦克射击
		bool tank_die[2][2] = { false,false,false,false };
		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id + 2] >= 4) {
				tank[enemy][id].shoot(actionlist[id + 2]);
				int xx = tank[enemy][id].x;
				int yy = tank[enemy][id].y;
				while (true) {
					xx += dx[actionlist[id + 2] - 4];
					yy += dy[actionlist[id + 2] - 4];
					if (xx < 0 || xx>8 || yy < 0 || yy>8 || state[yy][xx] == steel)
						break;
					if (state[yy][xx] == base || state[yy][xx] == brick) {
						destroylist.push_back(Pos{ xx, yy });
						break;
					}
					for (size_t i = 0; i < 2; i++)
					{
						if (tank[self][i].at(xx, yy)) {
							if (tank[self][1 - i] == tank[self][i]) {
								// 重叠坦克被射击立刻全炸
								tank_die[self][0] = true;
								tank_die[self][1] = true;
							}
							else if (tank[self][1 - i] == tank[enemy][1 - id]) {
								tank_die[self][1 - i] = true;
								tank_die[enemy][1 - id] = true;
							}
							else if (actionlist[id + 2] != opposite_shoot(actionlist[i])) {
								tank_die[self][i] = true;
							}

						}
					}
				}
			}
		}
		///处理己方坦克射击
		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id] >= 4) {
				tank[self][id].shoot(actionlist[id]);
				int xx = tank[self][id].x;
				int yy = tank[self][id].y;
				while (true) {
					xx += dx[actionlist[id] - 4];
					yy += dy[actionlist[id] - 4];
					if (xx < 0 || xx>8 || yy < 0 || yy>8 || state[yy][xx] == steel)
						break;
					if (state[yy][xx] == base || state[yy][xx] == brick) {
						destroylist.push_back(Pos{ xx, yy });
						break;
					}
					for (size_t i = 0; i < 2; i++)
					{
						if (tank[enemy][i].at(xx, yy)) {
							if (tank[enemy][1 - i] == tank[enemy][i]) {
								// 重叠坦克被射击立刻全炸
								tank_die[enemy][0] = true;
								tank_die[self][1] = true;
							}
							else if (tank[enemy][1 - i] == tank[self][1 - id]) {
								tank_die[enemy][1 - i] = true;
								tank_die[self][1 - id] = true;
							}
							else if (actionlist[id + 2] != opposite_shoot(actionlist[i])) {
								tank_die[enemy][i] = true;
							}

						}
					}
				}
			}
		}
		// 结算射击结果
		for (auto & i : destroylist)state[i.y][i.x] = none;
		for (size_t side = 0; side < 2; side++)
		{
			for (size_t id = 0; id < 2; id++)
			{
				if (tank_die[side][id])tank[side][id].die();
			}
		}

	}


	// 验证移动是否合法
	bool move_valid(int side, int id, int act)const
	{
		if (act == -1)return true;
		int xx = tank[side][id].x + dx[act];
		int yy = tank[side][id].y + dy[act];
		if (tank[side][1 - id].alive())
		{
			if (tank[side][1 - id].at(xx, yy))
				return false;
		}
		if (!tankOK(xx, yy))return false;
		for (int i = 0; i < 2; i++)
		{
			if (tank[1 - side][i].alive())//can not step into a enemy's tank's block (although tanks can overlap inadventently)
			{
				if (state[yy][xx] != forest && tank[1 - side][i].at(xx, yy))
					return false;
			}
		}
		return true;
	}

	// 射击粗剪枝
	// 射击没有摧毁物品、打不到敌人且没有危险性的都剪掉。
	bool shoot_valid(int side, int id, int act) const {
		if (!tank[side][id].can_shoot())return false;
		if (!tank[side][id].alive() && act != -1)return false;
		int xx = tank[side][id].x;
		int yy = tank[side][id].y;
		bool shoot_enemy = false;
		while (true) {
			xx += dx[act - 4];
			yy += dy[act - 4];
			if (xx < 0 || xx>8 || yy < 0 || yy>8 || state[yy][xx] == steel) {
				return shoot_enemy;
			}
			shoot_enemy |= possible_enemy(side, xx, yy, act - 4);
			if (state[yy][xx] == base) {
				if (yy == (side == blue ? 0 : 8))
					return false;
				else return true;
			}
			if (state[yy][xx] == brick) {
				return true;
			}
		}
	}
	///早期历史遗留，为了保持代码可读性现在用isOpposite，而且这个和tank类的方法重名了
	bool can_shoot(const Battlefield & f, int x, int y, int dst_x, int dst_y) const {
		if (x != dst_x && y != dst_y)return false;
		if (x == dst_x) {
			int y_step = y < dst_y ? 1 : -1;
			for (size_t i = y; i != dst_y; i += y_step)
			{
				if (f.state[i][x] == brick || f.state[i][x] == steel)return false;
			}
		}
		else {
			int x_step = x < dst_x ? 1 : -1;
			for (size_t i = x; i != dst_x; i += x_step)
			{
				if (f.state[y][i] == brick || f.state[y][i] == steel)return false;
			}
		}
		return true;
	}
	//locate_enemy using two-method : attack us or attack base
	void locate_enemy(vector<Battlefield> & cases)const {
		// 若仍有未知敌人未死亡
		// 直接随机死一个
		Battlefield tmp = *this;
		if (vague_death) {
			tmp.tank[enemy][rand() % 2].die();
			// 但不清空possible_field
		}
		vector<Pos> enemypos[2];
		for (size_t id = 0; id < 2; id++)
		{
			if (tmp.tank[enemy][id].in_forest()) {
				for (int i = 0; i < 9; i++)
				{
					for (int j = 0; j < 9; j++)
					{
						if (possible_field[id][i][j]) {
							enemypos[id].push_back(Pos{ j, i });
						}
					}
				}
			}
			else {
				enemypos[id].push_back(Pos{ tmp.tank[enemy][id].x,tmp.tank[enemy][id].y });
			}
		}

		tmp.tank[enemy][0].steps_in_forest = 0;
		tmp.tank[enemy][1].steps_in_forest = 0;
		for (size_t i = 0; i < enemypos[0].size(); i++)
		{
			for (size_t j = 0; j < enemypos[1].size(); j++)
			{
			    cout << i << ' '<< j << endl;
				Battlefield current = tmp;
				tmp.tank[enemy][0].x = enemypos[0][i].x;
				tmp.tank[enemy][0].y = enemypos[0][i].y;
				tmp.tank[enemy][1].x = enemypos[1][j].x;
				tmp.tank[enemy][0].y = enemypos[1][j].y;
				///current.eval_enemy_pos();
				cases.push_back(current);
			}
		}
	}

    bool can_attack_base(int side, int id)const {
        if (!tank[side][id].alive())return false;

        int x = 4, y = (self == red && side == enemy) || (self == blue && side == self) ? 8 : 0;
        return can_attack(side, id, x, y);
    }
    inline bool can_attack(int side, int id, int x, int y)const {
        return can_shoot(*this, tank[side][id].x, tank[side][id].y, x, y);
    }

    inline int cost_attack_base(int id, bool enemy = false) {
        int x = 4, y = (self&&enemy) || (!self && !enemy) ? 8 : 0;
        return cost_attack(id, enemy, x, y);
    }

	int cost_attack(int side, int id) {
	    ///side 自己是哪一方 id 坦克的id x y
	    ///只找到敌方基地的距离
	    print_state();
		struct point {
			int x, y;
			int cost;
			point(int xx, int yy, int _cost) :x(xx), y(yy), cost(_cost){}
			inline bool valid() { return x >= 0 && x < 9 && y >= 0 && y < 9; }
		};
		int dest_side = !side;
		int x = tank[side][id].x, y = tank[side][id].y;
		if(route_searched[dest_side]) return distance[dest_side][y][x];
		for(int i=0;i<9;i++){
		    for(int j=0;j<9;j++){
		        distance[dest_side][i][j] = INF;
		    }
		}
		queue<point> q;
		if(state[8*dest_side][4]==base or state[8*dest_side][4]==brick) distance[dest_side][8*dest_side][4] = 1;
        else distance[dest_side][8*dest_side][4] = 0;
        point start(4, 8*dest_side, distance[dest_side][8*dest_side][4]);
        for(int i=0;i<4;i++){
            int obstacle = 0;
            int xx = start.x + dx[i], yy = start.y + dy[i], cost = start.cost;
            while(inMap(xx,yy)){
                if(state[yy][xx] == steel) break;
                if(state[yy][xx] == water){
                    yy += dy[i], xx += dx[i];
                    continue;
                }
                if(state[yy][xx] == brick){
                    cost = obstacle * 2 + start.cost;
                    obstacle++;
                    distance[dest_side][yy][xx] = cost;
                    q.push(point(xx,yy,cost));
                }
                else{
                    cost = obstacle * 2 + start.cost;
                    distance[dest_side][yy][xx] = cost;
                    q.push(point(xx,yy,cost));
                }
                yy += dy[i], xx += dx[i];
            }
        }
        while(!q.empty())
        {
            point temp = q.front();
            q.pop();
            if(temp.cost > distance[dest_side][temp.y][temp.x]) continue;
            for(int i=0; i<4; i++){
                point next(0,0,0);
                next.x = temp.x + dx[i];
                next.y = temp.y + dy[i];
                if(!inMap(next.x, next.y)) continue;
                if((state[next.y][next.x] & steel) or (state[next.y][next.x] & water)){
                    distance[dest_side][next.y][next.x] = INF;
                    continue;
                }
                if(dest_side == 0){///敌方dest side是蓝方
                    if(next.y == 8 and next.x == 4){
                        distance[dest_side][next.y][next.x] = INF;
                        continue;
                    }///己方的基地当成steel处理
                }
                if(dest_side == 1){
                    if(next.y == 0 and next.x == 4){
                        distance[dest_side][next.y][next.x] = INF;
                        continue;
                    }
                }
                if((temp.y == tank[side][0].y and temp.x == tank[side][0].x) or (temp.y == tank[side][1].y and temp.x == tank[side][1].x)){
                    temp.cost += 1;
                }
                if(state[temp.y][temp.x] & brick) next.cost = temp.cost + 2;
                else next.cost = temp.cost + 1;
                if(next.cost < distance[dest_side][next.y][next.x]){
                    distance[dest_side][next.y][next.x] = next.cost;
                    q.push(next);
                }
            }
        }
        if(print_distance){
            for(int i=0;i<9;i++)
            {
                for(int j=0;j<9;j++)
                {
                    if(distance[dest_side][i][j] > 10000) cout << "F ";
                    else printf("%d ", distance[dest_side][i][j]);
                }
                cout << endl;
            }
        }
        return distance[dest_side][y][x];
	}
    ///重载后的函数，计算与任意一点的攻击距离，但是由于目标位置不确定不能像基地一样存下来减少计算
    int cost_attack(int side, int id, int x, int y){
        ///side 自己是哪一方 id   目标位置为x y
        ///只找到敌方基地的距离
        print_state();///debug
        struct point {
            int x, y;
            int cost;
            point(int xx, int yy, int _cost) :x(xx), y(yy), cost(_cost){}
            inline bool valid() { return x >= 0 && x < 9 && y >= 0 && y < 9; }
        };
        int dis[9][9];
        for(int i=0;i<9;i++){
            for(int j=0;j<9;j++){
                dis[i][j] = INF;
            }
        }
        queue<point> q;
        if(state[y][x]==brick) dis[y][x] = 1;
        else dis[y][x] = 0;
        point start(x,y,dis[y][x]);
        for(int i=0;i<4;i++){
            int obstacle = 0;
            int xx = start.x + dx[i], yy = start.y + dy[i], cost = start.cost;
            while(inMap(xx,yy)){
                if(state[yy][xx] == steel) break;
                if(state[yy][xx] == water){
                    yy += dy[i], xx += dx[i];
                    continue;
                }
                if(state[yy][xx] == brick){
                    cost = obstacle * 2 + start.cost;
                    obstacle++;
                    dis[yy][xx] = cost;
                    q.push(point(xx,yy,cost));
                }
                else{
                    cost = obstacle * 2 + start.cost;
                    dis[yy][xx] = cost;
                    q.push(point(xx,yy,cost));
                }
                yy += dy[i], xx += dx[i];
            }
        }
        while(!q.empty())
        {
            point temp = q.front();
            q.pop();
            if(temp.cost > dis[temp.y][temp.x]) continue;
            for(int i=0; i<4; i++){
                point next(0,0,0);
                next.x = temp.x + dx[i];
                next.y = temp.y + dy[i];
                if(!inMap(next.x, next.y)) continue;
                if((state[next.y][next.x] & steel) or (state[next.y][next.x] & water)){
                    dis[next.y][next.x] = INF;
                    continue;
                }
                if(side == 1){///自己是红方
                    if(next.y == 8 and next.x == 4){
                        dis[next.y][next.x] = INF;
                        continue;
                    }///己方的基地当成steel处理
                }
                if(side == 0){
                    if(next.y == 0 and next.x == 4){
                        dis[next.y][next.x] = INF;
                        continue;
                    }
                }
                if((temp.y == tank[side][0].y and temp.x == tank[side][0].x) or (temp.y == tank[side][1].y and temp.x == tank[side][1].x)){
                    temp.cost += 1;///避免己方坦克对己方另一辆坦克的路线进行干扰
                }
                if(state[temp.y][temp.x] & brick) next.cost = temp.cost + 2;
                else next.cost = temp.cost + 1;
                if(next.cost < dis[next.y][next.x]){
                    dis[next.y][next.x] = next.cost;
                    q.push(next);
                }
            }
        }
        if(print_distance){///debug部分
            for(int i=0;i<9;i++)
            {
                for(int j=0;j<9;j++)
                {
                    if(dis[i][j] > 10000) cout << "INF ";
                    else printf("%d ", dis[i][j]);
                }
                cout << endl;
            }
        }
        return dis[y][x];
    }
    inline bool overlap(Tank & self_tank, Tank & enemy_tank){
#ifdef DEBUG
        cout << "[checking overlap]:";
        if(self_tank.x == enemy_tank.x and self_tank.y == enemy_tank.y) cout << "yes" << endl;
        else cout << "no" << endl;
#endif
        return (self_tank.x == enemy_tank.x and self_tank.y == enemy_tank.y);
    }
    inline double forest_eval(int tmpside = self){
	    ///由于搜索时默认假设敌人知道我方位置，给在树林里的坦克额外加分以抵消
	    double eval;
	    for(int i=0;i<2;i++){
	        int x = tank[tmpside][i].x, y = tank[tmpside][i].y;
	        if(state[y][x] == forest) eval += 20;///para magic number
	        if(possible_field[0][y][x] or possible_field[1][y][x]){///己方有可能和敌方重叠

	        }
	    }
	    return eval;
	}
    inline double opposite_eval(int tmpside = self){
#ifdef DEBUG
        cout << "[processing opposite] " << endl;
#endif
        double eval = 0;
        ///para: 一般对射和二打一参数不同 还是交给geatpy调吧 三个都是magic number
        ///para_1和para_2加起来正好是坦克被消灭的惩罚
        double para_1 = 10, para_2 = 75 - para_1, para_3 = 5;
        bool opposite[2][2] = {false, false, false, false};
        bool can_shoot[2][2] = {true, true, true, true};
        ///opposite矩阵的[i][j]处为true代表己方坦克i与敌方坦克j相对
        ///can_shoot[side][i]表示side方的第i辆坦克能否射击
        for(int i=0;i<2;i++){
            for(int j=0;j<2;j++){
                opposite[i][j] = isOpposite(tank[tmpside][i], tank[!tmpside][j]);
                can_shoot[i][j] = (tank[i][j].n_fire_rounds>0);
            }
        }

        ///对己方坦克判断
        for(int i=0;i<2;i++){
            if(opposite[i][0] & opposite[i][1]){///自己被敌人夹击,几乎必死无疑
                eval -= para_1;///如果敌人有两辆甚至一辆坦克没有炮弹，大抵还有救， 这里减是额外惩罚
                if(can_shoot[!tmpside][0] and can_shoot[!tmpside][1]){///没救了，治不了，告辞
                    eval -= para_2;
                }
                else if(can_shoot[!tmpside][0] and can_shoot[!tmpside][1]){///只有一辆有炮弹，不至于必死
                    eval -= para_3;
                }
            }
            else if(opposite[i][0] || opposite[i][1]){///只和一辆坦克相对的情况
                eval -= 0.3;///para 又一个magic number 躲开总是好一点的
                int op = -1;///记录和己方i相对的敌方坦克的编号
                if(opposite[i][0]) op = 0;
                else op = 1;
                ///都有炮弹或者都没炮弹
                //if(can_shoot[tmpside][i]==can_shoot[!tmpside][op]) continue;
                ///己方有而敌方没有
                //else if(can_shoot[tmpside][i] > can_shoot[!tmpside][op]){
                //   eval += para_3;
                //}
                ///己方没有敌方有
                //else{
                //    eval -= para_3;
                //}
                ///这三个特判可以合到一个式子里 以上代码保留是为了易于理解
                eval += (double)(can_shoot[tmpside][i]-can_shoot[!tmpside][op]) * para_3;
            }
        }
        ///剩下只需要特判己方两辆坦克攻击对方一辆坦克就可
        ///这一部分效果不理想，给的权重太大导致优先于攻击基地
        /*for(int i=0;i<2;i++) {
            if (opposite[0][i] & opposite[1][i]) {
                eval += para_1;
                if (can_shoot[tmpside][0] and can_shoot[tmpside][1]) {
                    eval += para_2;
                }
                else if(can_shoot[tmpside][0] and can_shoot[tmpside][1]){
                    eval += para_3;
                }
            }
        }*/
        return eval;
    }
	double evaluate(bool tmpside = self){///针对某一方进行估值，默认对己
	    ///考虑到有用纳什均衡的可能性因此预留敌方
	    double value = 0;
        int _gameState = getGameState();
        if(_gameState == self) return INF;///己方胜利
        if(_gameState == !self) return -INF;///败北
        int self_dis[2] = {35,35}, enemy_dis[2]= {35,35}, self_min= 10000, enemy_min = 10000;///己方 敌方两辆坦克距离 己方 敌方最短距离
        for(int i=0;i<2;i++){
            if(!tank[tmpside][i].dead)
                self_dis[i] = cost_attack(tmpside, i);
            if(!tank[!tmpside][i].dead)
                enemy_dis[i] = cost_attack(!tmpside, i);
        }
        enemy_min = enemy_dis[0] < enemy_dis[1] ? enemy_dis[0] : enemy_dis[1];
        self_min = self_dis[0] < self_dis[1] ? self_dis[0] : self_dis[1]; ///两个距离中取最小
#ifdef DEBUG
        cout << "[self dis]: [0]: " << self_dis[0] << "  [1]: " << self_dis[1] << endl;
        cout << "[enemy dis]: [0]: " << enemy_dis[0] << "  [1]: " << enemy_dis[1] << endl;
#endif
        ///等有时间会写一份详细的文档解释为什么估值要这样
        ///估值：绝对距离之差加相对距离之差
        ///para : 50 50是为了保证非负  1.5 己方影响更大 0.2 纯粹magic number 1.0 离敌方基地近的坦克额外加分 5 截断值
        ///战略纵深作为辅助 作用远比距离小 但是可以在有向前走和向前开炮两种选择时选择开炮
        ///由于tank2s特殊性，depth是否保留再做考虑, 目前只是减小了权重
        value += 50  - 1.2 * (double)(self_dis[0] + self_dis[1]);
        value += 1.2 * sgn(enemy_min - self_min) * exp(enemy_min - self_min) / min(self_min, enemy_min);
        value += 0.3 * (double)(depth(tank[tmpside][1]) + depth(tank[tmpside][0]));
        value +=  1.0 * exp(5 - self_min);
        ///关于坦克如何移动到此结束
        ///战斗部分
        ///para: 有炮弹的情况比没有炮弹更优 但是也不能因此不射击墙壁 因此不应该大于战略纵深的参数
        value += 0.4 * double((tank[tmpside][0].n_fire_rounds!=0) + (tank[tmpside][1].n_fire_rounds!=0));
        value += opposite_eval();///对射的估值
        ///重叠情况的特殊处理, 己方坦克是可以重叠以采取一些神奇策略的，因此只判断是否和敌方重叠
        ///para: 这里也是magic number 距离的截断值2 和估值的影响5
        for(int i=0;i<2;i++){///己方i
            for(int j=0;j<2;j++){///敌方j
                if(overlap(tank[tmpside][i], tank[!tmpside][j])){
                    if(self_dis[i] - enemy_dis[j] >= -1) value -= 20;///这里采取保守策略，尽可能不出现重叠
                    else value += 5;///己方离得近的话大概可以冲了
                }
            }
        }
        return value;
	}
};
int Battlefield::stay = 0;
bool Battlefield::possible_field[2][9][9] = { 0 };

struct Bot {
	vector<Battlefield> round;
	bool long_time;
	inline bool longtime() { return long_time; }
	Bot(bool ltime = false) :long_time(ltime) {	}
	int last_act[2];
	///短时运行可能会有各种神奇bug
	void Update(Battlefield & f, Json::Value & request, int * responses) {
#ifdef DEBUG
	    cout << "round: " << f.rounds << endl;
	    cout << "[updating map]" << endl;
	    cout << "previous:" << endl;
	    f.print_state();
#endif
		//ready to build a single branch tree for evaluation
		f.rounds++;
		int self_destroy[4] = {-1,-1,-1,-1};///推断自己即将要摧毁的方块 y在前 x在后
		for(int j=0;j<2;j++){
		    if (!f.tank[self][j].alive()) {
                continue;
            }
		    int act = f.tank[self][j].action;
		    if(act < 4) continue;
		    int xx = f.tank[self][j].x + dx[act - 4], yy = f.tank[self][j].y + dy[act - 4];
            while(inMap(xx,yy)) {
                if (f.state[yy][xx] == base or f.state[yy][xx] == brick) {
                    self_destroy[2 * j] = yy, self_destroy[2 * j + 1] = xx;
                    break;
                }
                if (f.state[yy][xx] == water or f.state[yy][xx] == forest or f.state[yy][xx] == none) {
                    xx += dx[act - 4], yy += dy[act - 4];
                    continue;
                } else break;
            }
		}
        cout << "bricks to be destroyed:  ";
        for(int i=0;i<4;i++)
            cout << self_destroy[i] << ' ';
        cout << endl;
		for (int j = 0; j < 2; j++)
		{
			if (!f.tank[self][j].alive()) {
				continue;
			}
			int act = responses[j];
			if (act >= 4){
                f.tank[self][j].shoot(act);
			}
			else {
				if (act == -1)f.stay++;
				else f.stay = 0;
				f.tank[self][j].move(act);
			}
		}

		//update enenmy location and action
		for (size_t j = 0; j < 2; j++)
		{
			int last_act;
			bool in_bush = false;
			int last_x = f.tank[enemy][j].x;
			int last_y = f.tank[enemy][j].y;
			if (f.tank[enemy][j].alive())
				last_act = request["action"][j].asInt();
			else continue;
			for (size_t k = 0; k < 2; k++)
			{
				int pos = request["final_enemy_positions"][2 * j + k].asInt();
				if(pos!=-2) f.tank[enemy][j][k] = pos;
				else in_bush = true;
			}
            if(f.tank[enemy][j].x == -1){
                f.tank[enemy][j].die();
                memset(f.possible_field[j],0,sizeof(f.possible_field[j]));
                continue;
            }
			if (in_bush) {
				if (is_move(last_act)) {
					// 进入森林的第一回合，上一个回合的行动是可见的，位置可以确定
					int x = f.tank[enemy][j].x + dx[last_act],
						y = f.tank[enemy][j].y + dy[last_act];
					f.possible_field[j][y][x] = true;
					f.tank[enemy][j].n_fire_rounds++;
					f.tank[enemy][j].action = last_act;
					///把位置重置为-2
					f.tank[enemy][j].x = f.tank[enemy][j].y = -2;
				}
				else {
					// 在森林中一个回合以上
					f.tank[enemy][j].steps_in_forest++;
                    f.tank[enemy][j].action = last_act;
					// 如果在森林中超过5回合，认为对方在森林中静止不动
					// 这基于一个认知：蹲草的坦克通常不会蹲着蹲着改变策略
					if (f.tank[enemy][j].steps_in_forest > 5)
						last_act = -1;
				}
			}
			else {
				if (f.tank[enemy][j].action == -2) {
					// 敌方走出森林
					// 反推上回合行动
					int x = f.tank[enemy][j][0], y = f.tank[enemy][j][1];
					// 先横后纵，第一个有可能蹲的草就是敌方曾经的位置。
					// 由于草丛边缘形状简单，通常不会判断不准。
					int ny[2] = { y - 1 >= 0 ? y : 0,y + 1 <= 8 ? y + 1 : 8 };
					int nx[2] = { x - 1 >= 0 ? x - 1 : 0,x <= 8 ? x + 1 : 8 };
					if (f.possible_field[j][y][nx[0]] == true)last_act = 1;
					else if (f.possible_field[j][y][nx[1]] == true)last_act = 3;
					else if (f.possible_field[j][ny[0]][x] == true)last_act = 2;
					else if (f.possible_field[j][ny[1]][x] == true)last_act = 0;
				}
				// 此时敌方一定在森林外，开枪与否已知
				f.tank[enemy][j].setact(last_act);
				f.tank[enemy][j].n_fire_rounds++;
				memset(f.possible_field[j], false, 81 * sizeof(bool));
			}
		}
        for (size_t j = 0; j < 2; j++)
        {
            if (f.tank[enemy][j].steps_in_forest >= 1) {
                if (f.tank[enemy][j].action != -2) {
                    f.tank[enemy][j].steps_in_forest--;
                }
                else {
                    bool tmp_possible_field[9][9];
                    memcpy(tmp_possible_field, f.possible_field[j], sizeof(tmp_possible_field));
                    for (int m = 0; m < 9; m++)
                    {
                        for (int n = 0; n < 9; n++)
                        {
                            if (f.possible_field[j][m][n] == true && f.state[m][n] == forest) {
                                tmp_possible_field[m][n] = true;
                                int y[2] = { m - 1 >= 0 ? m - 1 : 0,m + 1 <= 8 ? m + 1 : 8 };
                                int x[2] = { n - 1 >= 0 ? n - 1 : 0,n + 1 <= 8 ? n + 1 : 8 };
                                tmp_possible_field[y[0]][n] |= (f.state[y[0]][n] == forest);
                                tmp_possible_field[y[1]][n] |= (f.state[y[1]][n] == forest);
                                tmp_possible_field[m][x[0]] |= (f.state[m][x[0]] == forest);
                                tmp_possible_field[m][x[1]] |= (f.state[m][x[1]] == forest);
                            }
                        }
                    }
                    cout << "DEBUG" << endl;
                    for (int m = 0; m < 9; m++)
                    {
                        for (int n = 0; n < 9; n++)
                        {
                            cout << tmp_possible_field[m][n];
                        }
                        cout << endl;
                    }
                    memcpy(f.possible_field[j], tmp_possible_field, sizeof(tmp_possible_field));
                }
            }

        }
		for (size_t j = 0; j < request["destroyed_tanks"].size(); j += 2)
		{
			int x = request["destroyed_tanks"][j].asInt(),
				y = request["destroyed_tanks"][j + 1].asInt();
			bool died = false;
			if (f.tank[self][0].at(x, y)) {
				f.tank[self][0].die();
				died = true;
			}
			if (f.tank[self][1].at(x, y)) {
				f.tank[self][1].die();
				died = true;
			}
			if (!died) {
				if (f.tank[enemy][0].alive() && f.tank[enemy][1].alive()) {
					// 我方坦克未死，敌方坦克居然也没死
					// 说明在树林里未成功处理
					if (!f.possible_field[0][y][x]) {
						// 0 坦克不在此树林 死的是1
						f.tank[enemy][1].die();
						memset(f.possible_field[1], false, 81 * sizeof(bool));
					}
					else if (!f.possible_field[1][y][x]) {
						// 同理
						f.tank[enemy][0].die();
						memset(f.possible_field[0], false, 81 * sizeof(bool));
					}
					else vague_death = true;
					// 否则不知死的是谁
				}
			}
			else if (f.tank[enemy][0].in_forest() || f.tank[enemy][1].in_forest()) {
				// 我方有坦克已死
				// 肯定不是自己杀的
				// 根据我方坦克死亡位置 判断敌方在森林中的位置
				f.set_forest_shooter(x, y);
			}

		}
        bool clear[2] = {false, false};///敌人只能一回合摧毁两个位置，除非开挂
        int mark = 0;
		for (size_t j = 0; j < request["destroyed_blocks"].size(); j += 2)
		{
			int x = request["destroyed_blocks"][j].asInt();
			int y = request["destroyed_blocks"][j + 1].asInt();
			f.state[y][x] = none;
			//shoot by self?
			if (!f.shoot_by_self(x, y)) {
			    cout << ">>>HERE!<<<" << endl;
				clear[mark++] = f.set_forest_shooter(x, y);
				f.print_possible();
				if(!clear[0] and clear[1]){
				    ///第一个砖块并不能确定而第二个可以，反过去推测第一个
                    int xx = request["destroyed_blocks"][j - 2].asInt();
                    int yy = request["destroyed_blocks"][j - 1].asInt();
				    f.set_forest_shooter(xx, yy);
				    f.print_possible();
				}
			}

		}
        for(int i=0;i<2;i++){

        }

		//current->valid_possiblefield();

	}


	Json::Value bot_main(Json::Value input)
	{
		Json::Reader reader;
		Json::Value temp, output;
		output = Json::Value(Json::arrayValue);
		round.push_back(Battlefield());
		if(round.size()==1) round[0].init(input);
		if (!long_time) {
			for (size_t i = 1; i < input["requests"].size(); i++)
			{
				int act[2] = {
					input["responses"][i - 1][0u].asInt(),
					input["responses"][i - 1][1].asInt() };
				Battlefield tmp = round[i - 1];
				Update(tmp, input["requests"][i], act);
				round.push_back(tmp);
			}
		}
		else {
			if (round.size() > 1) {
				Battlefield tmp = round[round.size() - 2];
				Update(tmp, input, last_act);
				round.push_back(tmp);
			}
		}
		round[round.size()-1].print_state();
		for(int i=0;i<round.size();i++)
        {
            round[i].print_possible();
            //round[i].evaluate();
        }
		Battlefield *pt = &round[round.size() - 1];
		vector<int> act[2];
		act[0].push_back(-1);
		act[1].push_back(-1);
		for (size_t id = 0; id < 2; id++)
		{
			for (size_t i = 0; i < 4; i++)
			{
				if (pt->move_valid(self, id, i))act[id].push_back(i);
			}
			for (size_t i = 4; i < 8; i++)
			{
				if (pt->shoot_valid(self, id, i))act[id].push_back(i);
			}
		}
        vector<Battlefield> cases;
		cases.push_back(*pt);
		cout << "________" << endl;
		pt->print_possible();
		pt->locate_enemy(cases);
		for(int i=0;i<cases.size();i++)
		    cases[i].print_state();
		//rand test;
#ifdef DEBUG
    cout << "[act0]: ";
    for(int i=0;i<act[0].size();i++)
        cout << act[0][i] << ' ';
	cout << endl << "[act1]: ";
	for(int i=0;i<act[1].size();i++)
	    cout << act[1][i] << ' ';
	cout << endl;
#endif
		Action max;
		max.a[0] = act[0][rand() % act[0].size()];
		max.a[1] = act[1][rand() % act[1].size()];
		if (long_time) {
			last_act[0] = max.a[0];
			last_act[1] = max.a[1];
		}
		
		output.append(max.a[0]);
		output.append(max.a[1]);

		return output;
	}
};



int main()
{
	srand(time(0));
	Json::Reader reader;
	Json::Value input, output;
	Json::FastWriter writer;
#ifndef _BOTZONE_ONLINE
	string s;
    if(manual_input) getline(cin, s);
	else s = string("{\"requests\":[{\"brickfield\":[23100456,13325976,10612788],\"forestfield\":[67239936,262400,513],\"mySide\":1,\"steelfield\":[787072,43008,655744],\"waterfield\":[0,33554434,0]}],\"responses\":[]}");

	reader.parse(s, input);
#else
	string s;
	getline(cin, s);
	reader.parse(s, input);
#endif

	
	Bot bot(false);
	output = bot.bot_main(input);
	cout << writer.write(output);
	while (bot.longtime()) {
		cout << "\n>>>BOTZONE_REQUEST_KEEP_RUNNING<<<\n";
		cout << flush;
		//getline(cin, s);// 舍弃request一行
		getline(cin, s);
		reader.parse(s, input);
		output = bot.bot_main(input);
		cout << writer.write(output);
	}


#ifndef _BOTZONE_ONLINE
	cout << clock();
	system("pause");
#endif
	return 0;
}
