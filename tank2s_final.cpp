#define _CRT_SECURE_NO_WARNINGS 1
//#define _BOTZONE_ONLINE
//#pragma GCC optimize(2)
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
#include <iomanip>
#include<cmath>
double enemy_average = 0;
int enemy_count = 0;
int eval_count = 0;
/**调试用控制变量**/
bool print_distance = false;
#ifndef _BOTZONE_ONLINE
bool manual_input = true;
#define nDEBUG
#define nquick
#define nPRINT_STATE
#endif
/**调试用控制变量**/

/////////////////// 参数表
//[eval_opposite]
double _1v2_pos_punishment = 8.75025; //处于1v2位置的惩罚 5-15
double _1v2_enemy1_punishment = 5; //敌人只有一方有炮弹的惩罚 1-5
double _tank_oppposite_punishment = 0.3;
//[evaluate]
    double _self_distance_weight = 0.601; //1-2
    double _sgn_exp_weight = 1.2;
    double _depth_weight = 0.518054; //纵深权重 0.1 - 0.6
    double _self_near_base_weight = 5; //用于扩大离敌方基地近的优势 0.5-2
    double _self_near_base_border = 7; // 算作"离敌方基地近”的标准 4-7
    double _can_fire_advantage = 0.4; // 有炮弹的优势
//[multipletank]
    double _multitank_advantage_border = 2; // 比敌方近*步，算作有明显优势
    double _multitank_far_punishment = 8.18353949; // 没明显优势，苟
    double _multitank_near_reward =  5; // 冲
//[intercept_ev]
    double _intercept_border = 9;
    double _intercept_punishment = 6;
    double _on_route_reward = 5;
    double _intercept_fire_reward = 3;
    double _intercept_stay_reward = 1.5;
    double _cost_attack_weight = 0.92474823; //0.2-1.2 //*

using namespace std;
const int dx[4] = { 0,1,0,-1 };
const int dy[4] = { -1,0,1,0 };
const int none = 0, brick = 1, steel = 2, water = 4, forest = 8, base = 16;
const int blue = 0, red = 1;
const int INF = 1919810;

inline bool is_move(int action) { return action >= 0 && action <= 3; }
inline bool is_shoot(int action) { return action >= 4; }
inline int opposite_shoot(int act) {
    return act > 5 ? act - 2 : act + 2;
}
inline int abs(int a) { return a > 0 ? a : -a; }
inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }

// 全局
int self, enemy;
bool vague_death = false;

///cmath里面竟然没有符号函数
inline int Manhattan(int x1, int y1, int x2, int y2){
    return abs(x1- x2) + abs(y1 - y2);
}
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
///敌方前100回合的所有行动 用于判断无限停止或者射击停止这样的循环
///卡大小开数组不是好文明
int prev_actions[105][2] = {{0}};
int prev_cooling[2] = {0,0};///如果某次成功发现了循环的话必定会进行利用打破平衡 这时候就不能再进行循环的识别 加一个冷却时间

struct Tank {
    static bool exposed[2];
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
	inline void die() { x = -1; y = -1; setact(-1); dead = true;}
	inline bool alive() const { return x != -1; }
	inline bool can_shoot()const {
		return n_fire_rounds > 0;
	}
	inline bool in_forest() { return x == -2; }///判断是否在树林里 只对对方
	inline bool at(int _x, int _y)const { return x == _x && y == _y; }
	int & operator[](int index) { if (index == 0)return x; else return y; }
	void move(int act) {
        if (act == -2) return;///虽然-2的确有用 但这个函数不是干这个的
	    action = act;
        n_fire_rounds++;
		if (act == -1) return;
		x += dx[act];
		y += dy[act];
	}
	void shoot(int act) { action = act; n_fire_rounds = 0;}
	bool operator==(const Tank & t)const { return x == t.x&&y == t.y; }
};
bool Tank::exposed[2] = {true, true};
struct Action{
    int a[2] = {0};
    double ev = -INF; ///估值
    double preference = 0;///行动被选中的概率
    ///如果只是minimax的话用不到preference
    int & operator [] (int k){
        return a[k];
    }
    bool operator<(Action b) const{
        return ev < b.ev;
    }
    bool operator>(Action b) const{
        return ev > b.ev;
    }
    int pack(){
        ///返回值范围为0到80 对应81*81的估值数组的索引
        ///-2明令禁止
        if(a[0] == -2 or a[1] == -2){
            cout << "ERROR!" << endl;
            return -1;///就是为了出错
        }
        return a[0] * 9 + a[1] + 10;
    }
};

struct Battlefield {

	int state[9][9];///地图
	static bool possible_field[2][9][9];///possible enemy position in forest
	bool route[9][9];
	int distance[2][9][9];
	bool route_searched[2] = {false, false};
	int rounds = 0;///回合数
    double value = 0;///局面估值, 暂时废弃
	static int stay;///暂时废弃，没有太大用处
	int iter_count = 0;///迭代次数
	Tank tank[2][2];
	vector<Action> self_act;
	vector<Action> enemy_act;
	double values[81][81] = {{0}};///第一个索引为己方action对应的packed act 第二个为敌方 只有己方敌方行动交叉的部分才可能有值
    void print_state(){
        ///用于调试输出地图
#ifdef PRINT_STATE
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
     void print_possible() const{
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
            if(state[y][x] == 0 or state[y][x] & forest){
                strategy_depth++;
                y += delta_y;
            }
            else break;
        }
        return strategy_depth;
    }

    bool dead_end(Tank & tmpTank)
    {
        bool tmpside = tmpTank.side;
        ///没有考虑被两辆坦克夹击的情况，以后可能会添加
        if (isOpposite(tmpTank,tank[1-tmpside][0]) or isOpposite(tmpTank,tank[1-tmpside][1]))
        {

            int op, yy, xx;//改动 弱化剪枝 只要不是打了砖块必死无疑还是可以打
            yy = tmpTank.y, xx = tmpTank.x;
            if(isOpposite(tmpTank,tank[1-tmpside][0])) op = 0;
            else op = 1;
            if(tank[1-tmpside][op].n_fire_rounds==0) return false;
            if(tank[1-tmpside][op].x == xx){//敌方和自己在同一竖线上
                if( (inMap(xx-1,yy) and state[yy][xx-1]==0) or (inMap(xx+1,yy) and state[yy][xx+1]==0) ){//只要左右两边还有地方可以躲，开炮也成
                    //      memcpy(state, state_back, sizeof(state));//复原地图
                    return false;//不用剪枝
                }
                else{
                    //        memcpy(state, state_back, sizeof(state));//复原地图
                    return true;//无路可逃，剪掉
                }
            }
            else if(tank[1-tmpside][op].y == yy){//敌方和自己在同一横线上,其实这种情况不太经常出现，实际上根本没遇到过
                if( (inMap(xx,yy-1) and state[yy-1][xx]==0) or (inMap(xx,yy+1) and state[yy+1][xx]==0) ){
                    //        memcpy(state, state_back, sizeof(state));//复原地图
                    return false;//不用剪枝
                }
                else{
                    //         memcpy(state, state_back, sizeof(state));//复原地图
                    return true;//无路可逃，剪掉
                }
            }
            //   memcpy(state, state_back, sizeof(state));//复原地图
            return false;
        }
        return false;
    }

    int min_dis(Tank & tank1, Tank & tank2){///计算两辆坦克x y方向距离中最短的一个
        int x, y;
        x = abs(tank1.x - tank2.x);
        y = abs(tank1.y - tank2.y);
        return min(x, y);
    }

    bool isOpposite(Tank & tank1, Tank & tank2)
    {
        if(tank1.dead or tank2.dead) return false;
        if (tank1.x != tank2.x and tank1.y != tank2.y)///不在同一直线上
            return false;
        if (tank1.x == tank2.x)
        {
            int x = tank1.x;
            int y1 = tank1.y, y2 = tank2.y;
            if (y1 == y2) return false;///在同一个格子上应该不算陷入江局
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
                else///中间有遮挡则不算对面
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
#ifdef DEBUG
	    cout << "[set_forest_shooter]: " << y << ' ' << x << endl;
#endif
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
		print_possible();
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
	///改动：传入的参数变为Actions的数组 Act[2] 为了方便搜索部分使用
	///需要保证不会传入-2
	void quick_play(Action act[2]) {
		tank[self][0].action = act[self][0];
		tank[self][1].action = act[self][1];
		tank[enemy][0].action = act[enemy][0];
		tank[enemy][1].action = act[enemy][1];
#ifdef quick
		cout << "-----------------------[before quick play]-------------------" << endl;
		print_state();
		cout << "self actions: " << "[0]:" << act[self][0] <<" [1]:"<<act[self][1]<< endl;
		cout << "enemy actions: " << "[0]:" << act[enemy][0] << " [1]:"<<act[enemy][1] << endl;
		cout << endl;
#endif

		for(size_t tmpside = 0;tmpside<2;tmpside++){
		    for(size_t tmp_id = 0; tmp_id < 0; tmp_id++){
		        if(tank[tmpside][tmp_id].dead)
                    tank[tmpside][tmp_id].killed = false;
		        ///如果一开始就死了，那必定不是被杀的
		    }
		}
		///处理坦克移动
        for (size_t id = 0; id < 2; id++)
        {
            if(act[self][id] < 4 and tank[self][id].alive())///活着并且能动
            {
                tank[self][id].move(act[self][id]);
            }
            if(act[enemy][id] < 4 and tank[enemy][id].alive())
            {
                tank[enemy][id].move(act[enemy][id]);
            }
        }
		///处理摧毁物体以及死亡坦克
		vector<Pos> destroylist;
		///处理敌方坦克射击
		bool tank_die[2][2] = { false,false,false,false };
        for (size_t id = 0; id < 2; id++)
        {
            if(act[enemy][id] >= 4){
                ///射击
                int ac = act[enemy][id];
                tank[enemy][id].shoot(ac);
                int xx = tank[enemy][id].x + dx[ac - 4];
                int yy = tank[enemy][id].y + dy[ac - 4];
                bool shot_tank  = false;
                while(inMap(xx,yy)){
                    if(state[yy][xx] == steel) break;
                    if (state[yy][xx] == base || state[yy][xx] == brick) {
                        destroylist.push_back(Pos{ xx, yy });
                        break;
                    }
                    if(tank[enemy][1-id].at(xx,yy)){///我 打 我 自 己
                        shot_tank = true;
                        if(tank[enemy][1-id] == tank[self][0]){
                            tank_die[enemy][1-id] = true, tank_die[self][0] = true;
                        }
                        else if(tank[enemy][1-id] == tank[self][1]){
                            tank_die[enemy][1-id] = true, tank_die[self][1] = true;
                        }
                        else if(!is_shoot(act[enemy][1-id]) or (is_shoot(act[enemy][1-id]) and act[enemy][id] != opposite_shoot(act[enemy][1-id]))){
                            tank_die[enemy][1-id] = true;
                        }
                    }
                    for (size_t i = 0; i < 2; i++)
                    {
                        if (tank[self][i].at(xx, yy)) {
                            shot_tank = true;
                            if (tank[self][1 - i] == tank[self][i]) {
                                // 重叠坦克被射击立刻全炸
                                tank_die[self][0] = true;
                                tank_die[self][1] = true;
                            }
                            else if (tank[self][1 - i] == tank[enemy][1 - id]) {
                                tank_die[self][1 - i] = true;
                                tank_die[enemy][1 - id] = true;
                            }
                            ///到这一步可以确定不会有重叠的坦克
                            else if ((act[enemy][id] != opposite_shoot(act[self][i]) and is_shoot(act[self][i])) or !is_shoot(act[self][i])) {
                                ///如果不是对射甚至己方没有射击
                                tank_die[self][i] = true;
                            }
                        }
                    }
                    if(shot_tank) break;
                    xx += dx[ac - 4], yy += dy[ac - 4];
                }
            }
        }
		///处理己方坦克射击
		for (size_t id = 0; id < 2; id++)
		{
			if (act[self][id] >= 4) {
			    int ac = act[self][id];
				tank[self][id].shoot(ac);
				int xx = tank[self][id].x + dx[ac - 4];
				int yy = tank[self][id].y + dy[ac - 4];
				bool shot_tank = false;
				while (inMap(xx,yy)) {
					if(state[yy][xx] == steel) break;
					if (state[yy][xx] == base || state[yy][xx] == brick) {
						destroylist.push_back(Pos{ xx, yy });
						break;
					}
                    if(tank[self][1-id].at(xx,yy)){///我 打 我 自 己
                        shot_tank = true;
                        if(tank[self][1-id] == tank[enemy][0]){
                            tank_die[self][1-id] = true, tank_die[enemy][0] = true;
                        }
                        else if(tank[self][1-id] == tank[enemy][1]){
                            tank_die[self][1-id] = true, tank_die[enemy][1] = true;
                        }
                        else if(!is_shoot(act[self][1-id]) or (is_shoot(act[self][1-id]) and act[self][id] != opposite_shoot(act[self][1-id]))){
                            tank_die[self][1-id] = true;
                        }
                    }
					for (size_t i = 0; i < 2; i++)
					{
						if (tank[enemy][i].at(xx, yy)) {
						    shot_tank = true;
							if (tank[enemy][1 - i] == tank[enemy][i]) {
								// 重叠坦克被射击立刻全炸
								tank_die[enemy][0] = true;
								tank_die[self][1] = true;
							}
							else if (tank[enemy][1 - i] == tank[self][1 - id]) {
								tank_die[enemy][1 - i] = true;
								tank_die[self][1 - id] = true;
							}
							else if(!is_shoot(act[enemy][i]) or ( is_shoot(act[enemy][i]) and act[self][id] != opposite_shoot(act[enemy][id])) ) {
								tank_die[enemy][i] = true;
							}

						}
					}
					if(shot_tank) break;
                    xx += dx[ac - 4], yy += dy[ac - 4];
				}
			}
		}
		// 结算射击结果
		for (auto & i : destroylist)state[i.y][i.x] = none;
		for (size_t side = 0; side < 2; side++)
		{
			for (size_t id = 0; id < 2; id++)
			{
				if (tank_die[side][id]){
                    tank[side][id].die();
                    tank[side][id].killed = true;
				}
			}
		}
#ifdef quick
		cout << "after quick play:" << endl;
		print_state();
		cout << "-----------------------------------" << endl;
#endif
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
	///打到自己也要剪掉
	///note：只建议用于己方 用于敌方会出bug
	bool shoot_valid(int side, int id, int act) const {
		if (!tank[side][id].can_shoot())return false;
		if (!tank[side][id].alive() && act != -1)return false;
		int xx = tank[side][id].x;
		int yy = tank[side][id].y;
		bool shoot_enemy = false;
		while (true) {
			xx += dx[act - 4];
			yy += dy[act - 4];
			if (!inMap(xx,yy) or state[yy][xx] == steel) {
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
			if(tank[side][1-id].at(xx,yy)){
			    return false;
			    ///不能打自己
			}
		}
	}
    void get_route(int id){
	    ///仅用来获取敌方某一辆坦克最短路线
	    ///最短路线可能会在途中变化
	    ///TLE警告
	    memset(route,0,sizeof(route));
	    int x = tank[enemy][id].x, y = tank[enemy][id].y;
	    while(inMap(x,y)){
	        if(distance[self][y][x]==1){
	            route[y][x] = true;
                break;
	        }
	        int dir = -1;
	        int min = 100;
	        for(int i = 0; i < 4; i++){
	            if(distance[self][y+dy[i]][x+dx[i]] < min){
	                min = distance[self][y+dy[i]][x+dx[i]];
	                dir = i;
	            }
	        }
	        x += dx[dir], y += dy[dir];
	        route[y][x] = true;
	    }
	}
	bool intercept(int id, int enemy_id){
	    ///判断己方的射击范围是否与敌方的行动路线交叉
	    int order[4];
	    if(self == blue){
	        order[0] = 2, order[1] = 3, order[2] = 1, order[3] = 0;
	    }///希望能提前返回减少一些计算量
	    else if(self == red){
	        order[0] = 0, order[1] = 1, order[2] = 3, order[3] = 2;
	    }
	    for(int i=0;i<4;i++){
	        int dir = order[i];
	        int x = tank[self][id].x, y = tank[self][id].y;
	        while(inMap(x,y)){
	            x += dx[dir], y += dy[dir];
	            if(route[y][x]) return true;
	            if(tank[enemy][enemy_id].at(x,y)) return true;
	            if(state[y][x] & steel or state[y][x] & brick) break;
	        }
	    }
	    return false;
	}
    void generate_move(int tmpside){
	    vector<Action> Actions;
	    vector<int> a[2];
	    if(tmpside == enemy){
            for(int id=0;id<2;id++){
                int temp = get_loop(id);
                //cout << "generate: " << temp << endl;
                if(temp == -2){
                    a[id].emplace_back(-1);
                    for (size_t i = 0; i < 4; i++) {
                        if (move_valid(tmpside, id, i)) a[id].emplace_back(i);
                    }
                    for (size_t i = 4; i < 8; i++) {
                        if (shoot_valid(tmpside, id, i)) a[id].emplace_back(i);
                    }
                }
                else{
                    a[id].emplace_back(temp);
                }
            }
	    }
	    else{
            for(int id=0;id<2;id++){
                a[id].emplace_back(-1);
                for (size_t i = 0; i < 4; i++) {
                    if (move_valid(tmpside, id, i)) a[id].emplace_back(i);
                }
                for (size_t i = 4; i < 8; i++) {
                    if (shoot_valid(tmpside, id, i)) a[id].emplace_back(i);
                }
            }
	    }
#ifdef DEBUG
	    cout << "[generate actions]";
	    for(int i=0;i<2;i++){
	        cout << "act" << i << ": ";
	        for(int j=0;j<a[i].size();j++){
	            cout << a[i][j] << ' ';
	        }
	        cout << endl;
	    }
#endif
        for(int i=0;i<a[0].size();i++){
            for(int j=0;j<a[1].size();j++){
                Action temp = *new Action;
                temp[0] = a[0][i];
                temp[1] = a[1][j];
                Actions.emplace_back(temp);///此时的action尚未估值 虽然估值到最后要存到别的地方
            }
        }
        if(tmpside == enemy){
            enemy_average += Actions.size();
            enemy_count++;
        }
        if(tmpside == self)
            self_act = Actions;
        else enemy_act = Actions;
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
#ifdef DEBUG
    cout << "[locate enemy]" << endl;
    print_possible();
#endif
        struct point{
            int x, y;
            int cost;
            point(int _x, int _y):x(_x),y(_y){}
            bool operator<(point b){
                return cost<b.cost;
            }
        };
		Battlefield tmp = *this;
		if (0) {
			tmp.tank[enemy][rand() % 2].die();
			// 但不清空possible_field
		}
		vector<point> enemypos[2];
		int delta = 0;
		///己方若是蓝方 处理敌人可能位置从上往下 否则从下往上
		if(self == 0) delta = 1;
		else if(self == 1) delta = -1;
		for (size_t id = 0; id < 2; id++)
		{
		    ///先行后列
			if (tmp.tank[enemy][id].in_forest()) {
				for (int i = 0; i < 9; i++)
				{
				    ///处理一列 同一列上至多允许两个 仍然按照离己方基地近处理
				    int count = 0;
					for (int j = (self == 0 ? 0 : 8); j < 9 and j >= 0; j += delta)
					{
					    if(count >= 2) break;
						if (possible_field[id][j][i]) {
						    count++;
							enemypos[id].emplace_back(point(i,j));
						}
					}
				}
			}
			else {
				enemypos[id].push_back(point(tmp.tank[enemy][id].x,tmp.tank[enemy][id].y));
			}
		}
		tmp.tank[enemy][0].steps_in_forest = 0;
		tmp.tank[enemy][1].steps_in_forest = 0;
		for(int i=0;i<2;i++){
		    if(enemypos[i].size()>4){
		        tmp.cost_attack(enemy, 0);
                for(auto & pos : enemypos[i]){
                    pos.cost = tmp.distance[self][pos.y][pos.x];
                }
                sort(enemypos[i].begin(), enemypos[i].end());
#ifdef DEBUG
                cout << "[sort by distance]" << endl;
                for(auto & pos : enemypos[i]){
                    cout << pos.cost << ' ';
                }
                cout << endl;
#endif
		    }
		}
		for (size_t i = 0; i < min(enemypos[0].size(), 4); i++)
		{
			for (size_t j = 0; j < min(enemypos[1].size(), 4); j++)
			{
				Battlefield current = tmp;
				current.tank[enemy][0].x = enemypos[0][i].x;
                current.tank[enemy][0].y = enemypos[0][i].y;
				current.tank[enemy][1].x = enemypos[1][j].x;
				current.tank[enemy][1].y = enemypos[1][j].y;
				///current.eval_enemy_pos();
				cases.emplace_back(current);
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
	    ///print_state();
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
                if(state[yy][xx] & steel) break;
                if(state[yy][xx] & water){
                    yy += dy[i], xx += dx[i];
                    continue;
                }
                if(state[yy][xx] & brick){
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
        ///print_state();///debug
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
                    if(dis[i][j] > 10000) cout << "F ";
                    else printf("%d ", dis[i][j]);
                }
                cout << endl;
            }
        }
        return dis[tank[side][id].y][tank[side][id].x];
    }
    inline bool overlap(Tank & self_tank, Tank & enemy_tank){
#ifdef DEBUG
        ///cout << "[checking overlap]:";
        ///if(self_tank.x == enemy_tank.x and self_tank.y == enemy_tank.y) cout << "yes" << endl;
        ///else cout << "no" << endl;
#endif
        if(state[self_tank.y][self_tank.x] & forest) return false;///都在森林中不算
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
        //cout << "[processing opposite] " << endl;
#endif
        double eval = 0;
        ///para_1和para_2加起来正好是坦克被消灭的惩罚
        double para_1 = _1v2_pos_punishment, para_2 = 75 - para_1, para_3 = _1v2_enemy1_punishment ;
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
        ///这一块容易导致自己太怂并且把敌人想的过于聪明
        for(int i=0;i<2;i++){
            if(opposite[i][0] & opposite[i][1]){///自己被敌人夹击,几乎必死无疑
                ///这个参数在某些情况挺重要 但是不很常用 可以考虑调参 范围0 - 15 优先其他几个 (geatpy)
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
                ///这里也是一个重要参数 一定程度上决定了bot到底有多怂 (geatpy) 建议范围0.5 —— 5
                eval += (double)(can_shoot[tmpside][i]-can_shoot[!tmpside][op]) * 2;///鉴于tank2的特殊情况给了一个很小的值让它不至于太怂
                if(can_shoot[!tmpside][op] and !can_shoot[tmpside][i])
                {
                    if(dead_end(tank[tmpside][i])){///如果坦克必死无疑
                        eval -= 35;
                    }
                }
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
    int get_loop(int id){
	    ///从敌人的行动陷入循环来推断敌人的下一回合行动
	    //cout << "get loop: " << prev_actions[rounds][id] << endl;
	    if(prev_cooling[id]<4) return -2;///冷却时间没到
	    for(int i=0;i<4;i++){
	        if(prev_actions[rounds-i][id] == -1){
	            if(i==3){
	                prev_cooling[id] = 0;///冷却时间重置
                    return -1;///连续三回合不动，可以猜测下回合也不会动
	            }
	            else continue;
	        }
	        else break;
	    }
	    int a1 = prev_actions[rounds-1][id], a2 = prev_actions[rounds][id];
	    for(int i=1;i<3;i++){
	        if(prev_actions[rounds-1-2*i][id]==a1 and prev_actions[rounds-2*i][id]==a2){
                continue;
	        }
	        else return -2;///没有检测到循环
	    }
	    //cout << "loop: " << a1 << ' ' << a2 << endl;
	    if(a1 == -1 or a2 == -1){
	        prev_cooling[id] = 0;
            return a1;
	    }
	    else return -2;
	}
	double evaluate(bool tmpside = self){///针对某一方进行估值，默认对己
	    ///考虑到有用纳什均衡的可能性因此预留敌方
	    ///好吧其实不用预留
	    double val = 0;
        int _gameState = getGameState();
        if(_gameState == self) return INF;///己方胜利
        if(_gameState == !self) return -INF;///败北
        if(_gameState == 2) return 0;///平局
        int self_dis[2] = {35,35}, enemy_dis[2]= {35,35}, self_min= 10000, enemy_min = 10000;///己方 敌方两辆坦克距离 己方 敌方最短距离
        for(int i=0;i<2;i++){
            if(!tank[tmpside][i].dead){
                if(!Tank::exposed[i]) val += 0.5;
                self_dis[i] = cost_attack(tmpside, i);
                val += 2.0/(double)Manhattan(tank[tmpside][i].x, tank[tmpside][i].y, 4, 8*(1-tmpside));///这个是类似微小扰动的东西 不用调
            }
            if(!tank[!tmpside][i].dead)
                enemy_dis[i] = cost_attack(!tmpside, i);
        }
        enemy_min = enemy_dis[0] < enemy_dis[1] ? enemy_dis[0] : enemy_dis[1];
        self_min = self_dis[0] < self_dis[1] ? self_dis[0] : self_dis[1]; ///两个距离中取最小
#ifdef DEBUG
        cout << "[self dis]: [0]: " << self_dis[0] << "  [1]: " << self_dis[1] << endl;
        cout << "[enemy dis]: [0]: " << enemy_dis[0] << "  [1]: " << enemy_dis[1] << endl;
#endif
        eval_count++;
#ifdef EV_DEBUG
        cout << "p1:" << val << endl;
#endif
        ///等有时间会写一份详细的文档解释为什么估值要这样
        ///估值：绝对距离之差加相对距离之差
        ///战略纵深作为辅助 作用远比距离小 但是可以在有向前走和向前开炮两种选择时选择开炮
        ///para 1.2 优先级最高的参数 范围大于1 小于2 (geatpy)
        val += 50  - _self_distance_weight * (double)(self_dis[0] + self_dis[1]);
        val += _sgn_exp_weight * sgn(enemy_min - self_min) * exp(enemy_min - self_min) / min( min(self_min, enemy_min) + 1 , 5 );///不能让敌人太近
        val += _depth_weight * (double)(depth(tank[tmpside][1]) + depth(tank[tmpside][0]));///2s的纵深可以适当地价值更高
        ///这个价值可以适当地大一些 大概 1 —— 5 的范围 (geatpy)
        val +=  _self_near_base_weight * exp(5 - self_min);///离得越近向前走的价值就越大
#ifdef EV_DEBUG
        cout << "p2:" << val << endl;
#endif
        ///关于坦克如何移动到此结束
        ///战斗部分
        ///para: 有炮弹的情况比没有炮弹更优 这个参数起到类似二选一的作用 可以不调或者手工调
        val += _can_fire_advantage * double((tank[tmpside][0].n_fire_rounds!=0) + (tank[tmpside][1].n_fire_rounds!=0));
        val += opposite_eval();///对射的估值
#ifdef EV_DEBUG
        cout << "p3:" << val << endl;
#endif
        ///重叠情况的特殊处理, 己方坦克是可以重叠以采取一些神奇策略的，因此只判断是否和敌方重叠
        ///para: 这里也是magic number 距离的截断值2 和估值的影响5
         for(int i=0;i<2;i++){///己方i
            for(int j=0;j<2;j++){///敌方j
                if(overlap(tank[myside][i], tank[!myside][j])){
                    if(self_dis[i] - enemy_dis[j] >= -_multitank_advantage_border) res -= _multitank_far_punishment;///这里采取保守策略，尽可能不出现重叠
                    else res += _multitank_near_reward;///己方离得近的话大概可以冲了
                }
            }
        }
        ///关于基地所处风险 这一部分可能和上面的神奇函数 0.8 * sgn(enemy_min - self_min) * exp(enemy_min - self_min) / min(self_min, enemy_min) 有一定的重复
        ///目前的想法时如果离基地的距离小于某个值则视为基地危险
        ///根据之前观看对局，这个情况权重应该相当大，足以抵消坦克所冒的风险
        ///这里是咕咕咕了两个星期的拦截模块
        ///主要思想很简单：如果对面的坦克离自己的距离比自己离对面近，就拦它
        for(int i=0;i<2;i++){
            if(rounds < 5) continue;
            if(enemy_min > 9) continue;
            if(self_dis[i] > enemy_dis[1-i]){///只考虑和自己相对的坦克
                val -= 6;///先扣分再说 加分不能超过总得分 除非算上cost attack
                get_route(1-i);
                if(route[tank[self][i].y][tank[self][i].x]){///自己在敌方坦克预计行动路线上
                    val += 5;
                }
                else if(intercept(i, 1-i)){///能够拦截敌人(至少震慑几回合
                    if(tank[self][i].n_fire_rounds > 0) val += 3;
                    else val += 1.5;
                }
                ///note：关于是否撤销离敌人距离的估值有待商榷 但是拦截状态确实无法对此作出有效贡献
                ///note: 对拦截行为有很大影响 建议调参 或者双方距离之差可以用非线性 当前估值的话范围0.2——1.2 (geatpy)
                val -= _cost_attack_weight*max(self_dis[i] - enemy_dis[1-i],4)*(double)cost_attack(tmpside, i, tank[1-tmpside][1-i].x, tank[1-tmpside][1-i].y);///防止距离太近时往旁边躲
                //val += 1.3 * (double)(self_dis[i]);
            }
        }
#ifdef EV_DEBUG
        cout << "p4:" << val << endl;
#endif
        ///还想做一个关于方块价值的，但不知道那个destroyed log怎么用
        ///para：坦克价值 暂时设为70
        for(int i=0;i<2;i++) {
            if (tank[tmpside][i].killed){
                ///关于这个的话我觉得可以调一下 但是必要性可能不大 如果用minimax
                if(!Tank::exposed[i]) val -= 30;///算是对己方在树林里作出补偿 因为敌方估值是按敌方上帝视角来算的
                else val -= 70;
            }
            if (tank[1-tmpside][i].killed) val += 70;
        }
#ifdef DEBUG
        cout << "value of this state:  " << val << endl;
#endif
        return val;
	}

	Action self_search(){
	    generate_move(self);
	    vector<Battlefield> possible_maps;
	    locate_enemy(possible_maps);
#ifdef DEBUG
        cout << "possible size: " << possible_maps.size() << endl;
        for(auto & i : possible_maps){
            cout << "possible state" << endl;
            i.print_state();
            cout << endl;
        }
#endif
	    for(int i = 0;i<self_act.size();i++){
	        double temp = 0;
	        for(int j=0;j<possible_maps.size();j++){
	            possible_maps[j].print_state();
	            temp += possible_maps[j].enemy_search(self_act[i]);
	        }
	        temp = temp / possible_maps.size();
	        self_act[i].ev = temp;
	    }
	    sort(self_act.begin(), self_act.end(), greater<>());
#ifdef DEBUG
        for(auto & i : self_act)
            cout << i[0] << ' ' << i[1] << ' ' << i.ev << endl;
#endif
	    return self_act[0];
	}
	double enemy_search(Action & act){
	    double ev = 114514;
	    generate_move(enemy);
#ifdef DEBUG
	    cout << "[enemy search] " << endl;
	    cout << "self act: " << act[0] << ' ' << act[1] << endl;
	    cout << "enemy moves size: " << enemy_act.size() << endl;
#endif
	    for(int i=0;i<enemy_act.size();i++){
            Battlefield temp = *this;
            double temp_ev;
            Action acts[2];
            acts[self] = act;
            acts[enemy] = enemy_act[i];
            temp.quick_play(acts);
            temp_ev = temp.evaluate(self);
            values[act.pack()][enemy_act[i].pack()] = temp_ev;
            if(temp_ev<ev) ev = temp_ev;
	    }
	    ///以自己的某一个行动在敌方所有行动下的平均值作为估值
	    //ev = ev / enemy_act.size();
	    ///minimax
	    return ev;
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
	///长时运行可能会有各种神奇bug
	void Update(Battlefield & f, Json::Value & request, int * responses) {
#ifdef DEBUG
	    cout << "round: " << f.rounds << endl;
	    cout << "[updating map]" << endl;
	    cout << "previous:" << endl;
	    f.print_state();
	    cout << "self action: " << responses[0] << ' ' << responses[1] << endl;
	    cout << "enemy action: " << request["action"][0u].asInt() << ' ' << request["action"][1].asInt() << endl;
#endif
        f.rounds++;
        prev_cooling[0]++, prev_cooling[1]++;
        ///更新prev actions
        prev_actions[f.rounds][0] = request["action"][0u].asInt();
        prev_actions[f.rounds][1] = request["action"][1].asInt();
        ///根据上一回合行动判断己方是否暴露
        for(size_t id = 0;id<2;id++){
            int xx = f.tank[self][id].x, yy = f.tank[self][id].y;
            if(!(f.state[yy][xx] & forest)){
                Tank::exposed[id] = true;
                continue;
            }
            if(f.tank[self][id].action >= 4){
                Tank::exposed[id] = true;
                continue;
            }
            else{
                Tank::exposed[id] = false;
            }
        }
        ///移动
        for(int j=0;j<2;j++){
            int act = responses[j];
            if(act < 4) {
                if (act == -1)f.stay++;
                else f.stay = 0;
                f.tank[self][j].move(act);
            }
        }
        ///追加判断：必须移动前后都在树林里面才可以算作没有暴露
        for(size_t id = 0;id<2;id++){
            int xx = f.tank[self][id].x, yy = f.tank[self][id].y;
            if(!(f.state[yy][xx] & forest)){
                Tank::exposed[id] = true;
                continue;
            }
        }
        //cout << "exposed" << Tank::exposed[0] << ' ' << Tank::exposed[1] << endl;

		int self_destroy[4] = {-1,-1,-1,-1};///推断自己即将要摧毁的方块 y在前 x在后
		for (int j = 0; j < 2; j++)
		{
			if (!f.tank[self][j].alive()) {
				continue;
			}
			int act = responses[j];
			if (act >= 4){
                f.tank[self][j].shoot(act);
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
		}

#ifdef DEBUG
        cout << "bricks to be destroyed:  ";
        for(int i=0;i<4;i++)
            cout << self_destroy[i] << ' ';
        cout << endl;
#endif
		//update enemy location and action
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
                    f.tank[enemy][j].n_fire_rounds++;
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
                        {///当前位置是树林，周围四个的树林也要更新为可能存在敌人
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
                    memcpy(f.possible_field[j], tmp_possible_field, 81*sizeof(bool));
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
					/// 我方坦克未死，敌方坦克居然也没死
					/// 说明在树林里未成功处理
					if (!f.possible_field[0][y][x]) {
						// 0 坦克不在此树林 死的是1
						f.tank[enemy][1].die();
						memset(f.possible_field[1], false, 81 * sizeof(bool));
					}
					else if (!f.possible_field[1][y][x]) {
						/// 同理
						f.tank[enemy][0].die();
						memset(f.possible_field[0], false, 81 * sizeof(bool));
					}
					else vague_death = true;
					/// 否则不知死的是谁
				}
			}
			else if (f.tank[enemy][0].in_forest() || f.tank[enemy][1].in_forest()) {
				/// 我方有坦克已死
				/// 肯定不是自己杀的
				/// 根据我方坦克死亡位置 判断敌方在森林中的位置
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
		///处理预计能够打中而实际没有打中的情况
        for(int i=0;i<2;i++){
            if(self_destroy[2*i] == -1) continue;
            bool really_destroyed = false;
            for(int j=0;j<request["destroyed_blocks"].size(); j += 2){
                int x = request["destroyed_blocks"][j].asInt();
                int y = request["destroyed_blocks"][j + 1].asInt();
                if(self_destroy[2*i] == y and self_destroy[2*i+1] == x){
                    really_destroyed = true;
                }
            }///判断预计会打中的方块是否在destroy list里面
            if(!really_destroyed)///那必定是被敌人挡住了
            {
                int act = f.tank[self][i].action;
                int xx = f.tank[self][i].x + dx[act - 4], yy = f.tank[self][i].y + dy[act - 4];
                bool possible_shooter[2] = {false, false};///有可能是哪一辆
                while(inMap(xx,yy)){
                   if(xx == self_destroy[2*i+1] and yy == self_destroy[2*i]) break;
                   if(f.possible_field[1][yy][xx]){
                       possible_shooter[1] = true;
                   }
                   if(f.possible_field[0][yy][xx]){
                       possible_shooter[0] = true;
                   }
                   if(possible_shooter[0] and possible_shooter[1]){
                       ///两辆都有可能，无法确定，不做处理
                       break;
                   }
                   xx += dx[act - 4], yy += dy[act - 4];
               }
               if(possible_shooter[0] != possible_shooter[1]){///有且只有一辆
                   int id;
                   if(possible_shooter[0]) id = 0;
                   else id = 1;
                   bool pos[9][9] = {{false}};
                   xx = f.tank[self][i].x + dx[act - 4], yy = f.tank[self][i].y + dy[act - 4];
                   while(inMap(xx,yy)){
                       if(xx == self_destroy[2*i+1] and yy == self_destroy[2*i]) break;
                       if(f.possible_field[id][yy][xx]){
                           pos[yy][xx] = true;
                       }
                       xx += dx[act - 4], yy += dy[act - 4];
                   }
                   memcpy(f.possible_field[id],pos,sizeof(pos));
               }
            }
        }
        for(int i=0;i<2;i++){
            if(f.tank[enemy][i].action>=4){
                f.tank[enemy][i].n_fire_rounds = 0;
            }
        }
#ifdef DEBUG
        cout << "[mapdata updated]" << endl;
        f.print_state();
        cout << "--------------function 'update' quitting------------" << endl << endl;
#endif
		//current->valid_possiblefield();

	}


	Json::Value bot_main(Json::Value input)
	{
	    int start, end;
	    start = clock();
		Json::Reader reader;
		Json::Value temp, output, debug;
		output = Json::Value(Json::arrayValue);
		debug = Json::Value(Json::arrayValue);
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
#ifdef DEBUG
		cout << "----------------current game state------------------" << endl;
		round[round.size()-1].print_state();
		cout << "----------------------------------------------------" << endl;
#endif
		Battlefield *pt = &round[round.size() - 1];
		pt->print_possible();
		vector<int> act[2];
		act[0].emplace_back(-1);
		act[1].emplace_back(-1);
		for (size_t id = 0; id < 2; id++)
		{
			for (size_t i = 0; i < 4; i++)
			{
				if (pt->move_valid(self, id, i))act[id].emplace_back(i);
			}
			for (size_t i = 4; i < 8; i++)
			{
				if (pt->shoot_valid(self, id, i))act[id].emplace_back(i);
			}
		}
		//rand test;
#ifdef DEBUG
    cout << "n fire rounds 0: " << pt->tank[self][0].n_fire_rounds << endl;
    cout << "[act0]: ";
    for(int i=0;i<act[0].size();i++)
        cout << act[0][i] << ' ';

	cout << endl << "n fire rounds 1: " << pt->tank[self][1].n_fire_rounds << endl << "[act1]: ";
	for(int i=0;i<act[1].size();i++)
	    cout << act[1][i] << ' ';
	cout << endl;
#endif
/*	    srand(pt->rounds);
		Action max;
		max.a[0] = act[0][rand() % act[0].size()];
		max.a[1] = act[1][rand() % act[1].size()];
		if (long_time) {
			last_act[0] = max.a[0];
			last_act[1] = max.a[1];
		}
		
		output.append(max.a[0]);
		output.append(max.a[1]);*/
        Action best;
        best = pt->self_search();
        output.append(best[0]);
        output.append(best[1]);
        last_act[0] = best[0];
        last_act[1] = best[1];
        end = clock();
        int time = (end-start)*1000/CLOCKS_PER_SEC;
        debug.append(time);
        output.append(debug);
		return output;
	}
};



int main()
{
    ///为调试长时运行，改为用回合数当种子
	///srand(time(0));
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

	
	Bot bot(true);
	long long a = clock();
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
	cout << enemy_average / enemy_count << endl;
	cout << eval_count << endl;
	cout << clock() - a;
	for(int i=0;i<100;i++){
	    cout << prev_actions[i][0] << ' ' << prev_actions[i][1] << endl;
	}
	system("pause");
#endif
	return 0;
}