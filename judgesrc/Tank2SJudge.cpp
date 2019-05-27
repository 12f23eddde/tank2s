// Tank2S 本地裁判程序
// 根据Bot修改而来
// 由于无需与平台交互，非常精简
// 且裁判默认为长时运行

#include"Bot.h"
#include"Bot1.h"

#define _CRT_SECURE_NO_WARNINGS 1
#define _BOTZONE_ONLINE

#include<vector>
#include<queue>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include "jsoncpp/json.h"

namespace judge {
	using namespace std;
	const int dx[4] = { 0,1,0,-1 };
	const int dy[4] = { -1,0,1,0 };
	const int none = 0, brick = 1, steel = 2, water = 4, forest = 8, base = 16;
	const int blue = 0, red = 1, nowinner = 2, game_continue = 3;
	inline bool CoordValid(int x, int y)
	{
		return x >= 0 && x < 9 && y >= 0 && y < 9;
	}

	inline bool is_move(int action) { return action >= 0 && action <= 3; }
	inline bool is_shoot(int action) { return action >= 4; }
	inline int opposite_shoot(int act) { 
		return act > 5 ? act - 2 : act + 2; 
	}
	inline int opposite_action(int act) {
		if (act == -1)return -1;
		if (act == -2)return -2;
		if (act >= 0 && act <= 3)return act > 1 ? act - 2 : act + 2;
		return act > 5 ? act - 2 : act + 2;
	}

	// 全局
	int self = blue, enemy = red;
	bool vague_death = false;
	// 统计变量
	int blue_tank_die = 0;
	int red_tank_die = 0;

	struct Pos {
		int x, y;
		bool operator <(const Pos & p)const { return x*9+ y<p.x*9+p.y; }
	};

	struct Action {
		int a[2];
	};

	
	struct Tank {
		int x, y;
		int action = -1;
		bool alive = true;
		inline void setpos(int _x, int _y) {
			x = _x, y = _y;
		}

		inline void setact(int act) { action = act; }
		inline void die() {
			alive = false;
		}
		inline bool can_shoot()const {
			return action < 4;
		}
		inline bool at(int _x, int _y)const { return x == _x && y == _y; }
		int & operator[](int index) { if (index == 0)return x; else return y; }
		void move(int act) {
			action = act;
			if (act >= 4)assert(0);
			if (act == -1)return;
			x += dx[act];
			y += dy[act];
		}
		void shoot(int act) { action = act; }
		bool operator==(const Tank & t)const { return x == t.x&&y == t.y; }
	};



	struct Battlefield {
		int gamefield[9][9];//main_field
		int steps = 0;
		int value = 0;
		static int stay;
		Tank tank[2][2];
		vector<Pos> destroyed_blocks;
		vector<Tank> destroyed_tanks;
		Tank prev_tanks[2][2];
		void init_tank() {
			tank[blue][0].setpos(2, 0);
			tank[blue][1].setpos(6, 0);
			tank[red][0].setpos(6, 8);
			tank[red][1].setpos(2, 8);
		}

		Battlefield & operator=(const Battlefield & b) {
			memcpy(gamefield, b.gamefield, sizeof(gamefield));
			memcpy(tank, b.tank, sizeof(tank));
			steps = b.steps;
			return *this;
		};
		Battlefield(const Battlefield &b) {
			*this = b;
		}
		~Battlefield() { }
		bool has_tank(int x, int y)const {
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank[side][id].alive&&tank[side][id].at(x, y))
					{
						return true;
					}
				}
			}
			return false;
		}
		// 对称检测
		bool ValidSymetry() {
			// 局面对称性
			
			int * start = &gamefield[0][0];
			int * end = &gamefield[8][8];
			for (size_t i = 0; i < 40; i++)
			{
				if (*start != *end)
					return false;
				start++, end--;
			}
			// 坦克对称性
			if (tank[blue][0].x + tank[red][0].x != 8 ||
				tank[blue][0].y + tank[red][0].y != 8 ||
				tank[blue][1].y + tank[red][1].y != 8 ||
				tank[blue][1].x + tank[red][1].x != 8) {
				return false;
			}
				
			if (tank[blue][0].action != opposite_action(tank[red][0].action) ||
				tank[blue][0].action != opposite_action(tank[red][0].action) ||
				tank[blue][1].action != opposite_action(tank[red][1].action) ||
				tank[blue][1].action != opposite_action(tank[red][1].action))
				return false;
			return true;
		}
		int cnt_tank(int x, int y)const {
			if (!tankOK(x, y))assert(0);
			int result = 0;
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank[side][id].alive&&tank[side][id].at(x, y))
					{
						result++;
					}
				}
			}
			return result;
		}

		bool operator<(const Battlefield & b) { return value < b.value; }
		bool in_forest(Tank & t) {
			return gamefield[t.y][t.x] == forest;
		}
		void init(Json::Value & info) {
			memset(gamefield, 0, 81 * sizeof(int));
			gamefield[0][4] = gamefield[8][4] = base;
			Json::Value requests = info["requests"], responses = info["responses"];
			//assert(requests.size());
			//load battlefield info
			init_tank();
			for (unsigned j = 0; j < 3; j++)
			{
				int x = requests[0u]["brickfield"][j].asInt();
				for (int k = 0; k < 27; k++)gamefield[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * brick;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = requests[0u]["forestfield"][j].asInt();
				for (int k = 0; k < 27; k++) {
					gamefield[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * forest;
				}
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = requests[0u]["steelfield"][j].asInt();
				for (int k = 0; k < 27; k++)gamefield[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * steel;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = requests[0u]["waterfield"][j].asInt();
				for (int k = 0; k < 27; k++)gamefield[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * water;
			}
		}

		Battlefield() {	}

		bool tankOK(int x, int y)const// can a tank steps in?
		{
			return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (gamefield[y][x] == none || gamefield[y][x] == forest);
		}

		Json::Value get_response(int side) {
			Json::Value result;
			//Json::FastWriter writer;
			result.append(tank[side][0].action);
			result.append(tank[side][1].action);
			//cout<< writer.write(result);
			return result;
		}


		Json::Value get_request(int side) {
			int enemy_act[2];
			if (in_forest(prev_tanks[1 - side][0])) {
				enemy_act[0] = -2; 
			}
			else if (!tank[1 - side][0].alive) {
				enemy_act[0] = -1;
			}
			else {
				enemy_act[0] = tank[1 - side][0].action;
			}
			if (in_forest(prev_tanks[1 - side][1])) {
				enemy_act[1] = -2;
			}
			else if (!tank[1 - side][1].alive) {
				enemy_act[1] = -1;
			}
			else {
				enemy_act[1] = tank[1 - side][1].action;
			}
			
			Json::Value request;
			request["action"].append(enemy_act[0]);
			request["action"].append(enemy_act[1]);
			for (auto & i:destroyed_blocks)
			{
				request["destroyed_blocks"].append(i.x);
				request["destroyed_blocks"].append(i.y);
			}
			for (auto & i : destroyed_tanks)
			{
				request["destroyed_tanks"].append(i.x);
				request["destroyed_tanks"].append(i.y);
			}
			for (size_t i = 0; i < 2; i++)
			{
				int enemy_pos[2] = { tank[1 - side][i].x ,tank[1 - side][i].y };
				if (!tank[1 - side][i].alive) {
					request["final_enemy_positions"].append(-1);
					request["final_enemy_positions"].append(-1);
				}
				else if (in_forest(tank[1 - side][i])) {
					request["final_enemy_positions"].append(-2);
					request["final_enemy_positions"].append(-2);
				}
				else {
					request["final_enemy_positions"].append(enemy_pos[0]);
					request["final_enemy_positions"].append(enemy_pos[1]);
				}
			}
			//Json::FastWriter writer;
			//cout << writer.write(request);
			return request;
		}


		void debug_print() {
			for (size_t i = 0; i < 9; i++)
			{
				for (size_t j = 0; j < 9; j++)
				{
					

					cout << endl;
				}
			}
			cout << endl;
		}

		// quick_play
		// 不检查合法性
		void quick_play(int * actionlist) {
			tank[self][0].action = actionlist[0];
			tank[self][1].action = actionlist[1];
			tank[enemy][0].action = actionlist[2];
			tank[enemy][1].action = actionlist[3];
			for (size_t id = 0; id < 2; id++)
			{
				if (!tank[self][id].alive)continue;
				if (actionlist[id] >= 4)continue;
				tank[self][id].move(actionlist[id]);
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (!tank[enemy][id].alive)continue;
				if (actionlist[id+2] >= 4)continue;
				tank[enemy][id].move(actionlist[id+2]);
			}
			set<Pos> destroylist;
			bool tank_die[2][2] = { false,false,false,false };
			// 裁判需要处理所有情形 包括自杀
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (!tank[side][id].alive)continue;
					if (actionlist[2 * side + id] < 4)continue;
					tank[side][id].shoot(actionlist[2 * side + id]);
					int xx = tank[side][id].x;
					int yy = tank[side][id].y;
					while (true) {
						xx += dx[actionlist[2 * side + id] - 4];
						yy += dy[actionlist[2 * side + id] - 4];
						if (gamefield[yy][xx] == water)continue;
						if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
							break;
						if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
							/*
							if (gamefield[yy][xx] == base) {
								
								if (side == blue && yy == 0)cout << "蓝方自杀\n";
								if (side == red && yy == 8)cout << "红方自杀\n";
							}
							*/
							destroylist.insert(Pos{ xx, yy });
							break;
						}
						
						int tank_cnt = cnt_tank(xx, yy);
						if (tank_cnt == 0)continue;;
						for (size_t dst_side = 0; dst_side < 2; dst_side++)
						{
							for (size_t dst_id = 0; dst_id < 2; dst_id++)
							{
								if (dst_side == side && dst_id == id)continue;
								if (!tank[dst_side][dst_id].alive)continue;
								if (!tank[dst_side][dst_id].at(xx, yy))continue;
								if (tank_cnt > 1) {
									tank_die[dst_side][dst_id] = true;
									if (dst_side == blue)blue_tank_die++;
									else red_tank_die++;
								}
								if (tank_cnt == 1) {
									if (actionlist[2 * dst_side + dst_id]>=4&&actionlist[2*side+id] != opposite_shoot(actionlist[2*dst_side+dst_id])) {
										tank_die[dst_side][dst_id] = true;
										if (dst_side == blue)blue_tank_die++;
										else red_tank_die++;
									}
								}
							}
						}
						break;
					}
				}
			}
									
			destroyed_tanks.clear();
			destroyed_blocks.clear();
			// 结算射击结果
			for (auto & i : destroylist) {
				gamefield[i.y][i.x] = none;
				destroyed_blocks.push_back(i);
			}
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank_die[side][id]) {
						destroyed_tanks.push_back(tank[side][id]);					
						tank[side][id].die();
						//cout << "Tank[" << side << "][" << id << "] died!";
					}
				}
			}

		}

		int play(int * actionlist) {
			for (size_t side = 0; side < 2; side++)
			{
				prev_tanks[side][0] = tank[side][0];
				prev_tanks[side][1] = tank[side][1];
			}
			bool lose[2] = { false,false };
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(blue, id, actionlist[id]))
				{
					lose[blue] = true;

					cout << "DEBUG信息：蓝方行为不合法\n";
					cout << "Tank id: " << id << "  Action: " << actionlist[id] << endl;
				}
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(red, id, actionlist[id + 2]))
				{
					lose[red] = true;
					cout << "DEBUG信息：红方行为不合法\n";
					cout << "Tank id: " << id << "  Action: " << actionlist[id + 2] << endl;
				}
			}
			if (lose[blue] && lose[red])return nowinner;
			if (lose[red])return blue;
			if (lose[blue])return red;
			quick_play(actionlist);
			bool baseAlive[2] = { gamefield[0][4] != none,gamefield[8][4] != none };
			bool tankAlive[2] = { tank[blue][0].alive || tank[blue][1].alive,
				tank[red][0].alive || tank[red][1].alive };
			lose[blue] = !baseAlive[blue] || !tankAlive[blue];
			lose[red] = !baseAlive[red] || !tankAlive[red];
			if (lose[blue] && lose[red])
				return nowinner;
			if (lose[red])
				return blue;
			if (lose[blue])
				return red;
			return game_continue;
		}

		// 验证移动是否合法
		bool move_valid(int side, int id, int act)const
		{
			if (!tank[side][id].alive && act != -1) {
				if(act<4)cout << "裁判警告：死亡的坦克试图移动" << endl;
			}
			if (act == -1)return true;
			if (act > 3)return true;
			int xx = tank[side][id].x + dx[act];
			int yy = tank[side][id].y + dy[act];
			if (tank[side][1 - id].alive)
			{
				if (tank[side][1 - id].at(xx, yy))
					return false;
			}
			if (!tankOK(xx, yy))return false;
			for (int i = 0; i < 2; i++)
			{
				if (tank[1 - side][i].alive)//can not step into a enemy's tank's block (although tanks can overlap inadventently)
				{
					if (gamefield[yy][xx] != forest && tank[1 - side][i].at(xx, yy))
						return false;
				}
			}
			return true;
		}
	};
	const int sideCount = 2, tankPerSide = 2;
	const int baseX[sideCount] = { 4,4 };
	const int baseY[sideCount] = { 0, 8 };
	const int tankX[sideCount][tankPerSide] = { { 2, 6 },{ 6 , 2 } };
	const int tankY[sideCount][tankPerSide] = { { 0, 0 },{ 8, 8} };
	const int Up = 0, Right = 0, Down = 0, Left = 0;
	int dirEnumerateList[24][4] = {
		{ Up, Right, Down, Left },	{ Up, Right, Left, Down },	{ Up, Down, Right, Left },
	{ Up, Down, Left, Right },	{ Up, Left, Right, Down },	{ Up, Left, Down, Right },
	{ Right, Up, Down, Left },	{ Right, Up, Left, Down },	{ Right, Down, Up, Left },
	{ Right, Down, Left, Up },	{ Right, Left, Up, Down },	{ Right, Left, Down, Up },
	{ Down, Up, Right, Left },	{ Down, Up, Left, Right },	{ Down, Right, Up, Left },
	{ Down, Right, Left, Up },	{ Down, Left, Up, Right },	{ Down, Left, Right, Up },
	{ Left, Up, Right, Down },	{ Left, Up, Down, Right },	{ Left, Right, Up, Down },
	{ Left, Right, Down, Up },	{ Left, Down, Up, Right },	{ Left, Down, Right, Up }
	};
	struct Judge {
		// Judge 主结构
		// 本Judge主要提供批量训练 没有复盘功能
		// 不支持botzone
		// 如有需要，可改为vector<Battlefield>存储每回合信息
		Battlefield field;
		int round = 0;
		Json::Value input;
		Json::Value output;
		int game_result = 0;
		bool long_time[2] = { false,false };
		int current_seed = 0;

		// 局面数组 这些数组在initfield中被一一设置
	
		int fieldBinary[3];
		int waterBinary[3];
		int steelBinary[3];
		int forestBinary[3];


		//int fieldBinary[3];
		//int waterBinary[3];
		//int steelBinary[3];
		//int forestBinary[3];
		// 标准局面生成器
		// 这个辣鸡生成器用了很多全局变量
		// 另外他的rand()很慢 可能成为性能瓶颈

		void InitializeField()
		{
			memset(fieldBinary, 0, sizeof(fieldBinary));
			memset(waterBinary, 0, sizeof(waterBinary));
			memset(steelBinary, 0, sizeof(steelBinary));
			memset(forestBinary, 0, sizeof(forestBinary));
			current_seed = time(0)+clock();
			srand(current_seed);
			bool hasBrick[9][9] = {};
			bool hasWater[9][9] = {};
			bool hasSteel[9][9] = {};
			bool hasForest[9][9] = {};
			int portionH = (9 + 1) / 2;
			do {//optimistic approach: assume that disconnectivity will not appear normally
				int y = rand() + rand();
				int f_posy = rand() % (portionH - 2);
				int f_posx = rand() % 9;
				//cout<<"Forest "<<f_posx<<' '<<f_posy<<endl;
				if (rand() % 4 == 0 && 0)//area is 2 * 2
				{
					for (int y = 0; y <= 1; y++)
						for (int x = 0; x <= 1; x++)
							color_block(hasForest, f_posy + y, f_posx + x);
				}
				else if (rand() % 4 == 0 && 0)//area is 2 * 3
				{
					for (int y = 0; y <= 1; y++)
						for (int x = 0; x <= 2; x++)
							color_block(hasForest, f_posy + y, f_posx + x);
				}
				else//area is 4 * 5
				{
					for (int y = 0; y <= 4; y++)
						for (int x = 0; x <= 5; x++)
							color_block(hasForest, f_posy + y, f_posx + x);
				}
				for (int y = 0; y < portionH; y++)
					for (int x = 0; x < 9; x++) {
						hasBrick[y][x] = false;
						if (!hasForest[y][x])
							hasBrick[y][x] = rand() % 3 > 1;// 1/3 brick
						if (hasForest[y][x] == false && hasBrick[y][x] == false)hasWater[y][x] = rand() % 27 > 22; // (3/4)*(4/27)= 1/9 water
						if (hasForest[y][x] == false && hasBrick[y][x] == false && hasWater[y][x] == false)hasSteel[y][x] = rand() % 23 > 18;//(3/4)*(23/27)*(4/23)=1/9 steel
					}
				int bx = baseX[0], by = baseY[0];
				hasBrick[by + 1][bx + 1] = hasBrick[by + 1][bx - 1] =
					hasBrick[by][bx + 1] = hasBrick[by][bx - 1] = true;
				hasWater[by + 1][bx + 1] = hasWater[by + 1][bx - 1] =
					hasWater[by][bx + 1] = hasWater[by][bx - 1] = false;
				hasSteel[by + 1][bx + 1] = hasSteel[by + 1][bx - 1] =
					hasSteel[by][bx + 1] = hasSteel[by][bx - 1] = false;
				hasForest[by + 1][bx + 1] = hasForest[by + 1][bx - 1] =
					hasForest[by][bx + 1] = hasForest[by][bx - 1] = false;
				hasBrick[by + 1][bx] = true;
				hasBrick[by][bx] = hasBrick[by][bx + 2] = hasBrick[by][bx - 2] = false;
				hasWater[by][bx] = hasWater[by + 1][bx] = hasWater[by][bx + 2] = hasWater[by][bx - 2] = false;
				hasForest[by][bx] = hasForest[by + 1][bx] = hasForest[by][bx + 2] = hasForest[by][bx - 2] = false;
				hasSteel[by][bx] = hasSteel[by + 1][bx] = hasSteel[by][bx + 2] = hasSteel[by][bx - 2] = false;
				//symmetry
				for (int y = 0; y < portionH; y++)
					for (int x = 0; x < 9; x++) {
						hasBrick[9 - y - 1][9 - x - 1] = hasBrick[y][x];
						hasWater[9 - y - 1][9 - x - 1] = hasWater[y][x];
						hasSteel[9 - y - 1][9 - x - 1] = hasSteel[y][x];
						hasForest[9 - y - 1][9 - x - 1] = hasForest[y][x];
					}
				//separate the field into 4 pieces, each with a tank
				for (int y = 2; y < 9 - 2; y++) {
					hasBrick[y][9 / 2] = true;
					hasWater[y][9 / 2] = false;
					hasSteel[y][9 / 2] = false;
					hasForest[y][9 / 2] = false;
				}
				for (int x = 0; x < 9; x++) {
					hasBrick[9 / 2][x] = true;
					hasWater[9 / 2][x] = false;
					hasSteel[9 / 2][x] = false;
					hasForest[9 / 2][x] = false;
				}
				for (int side = 0; side < sideCount; side++)
				{
					for (int tank = 0; tank < tankPerSide; tank++)
						hasForest[tankY[side][tank]][tankX[side][tank]] = hasSteel[tankY[side][tank]][tankX[side][tank]] = hasWater[tankY[side][tank]][tankX[side][tank]] = false;
					hasForest[baseY[side]][baseX[side]] = hasSteel[baseY[side]][baseX[side]] = hasWater[baseY[side]][baseX[side]] = hasBrick[baseY[side]][baseX[side]] = false;
				}
				//add steel onto midpoint&midtankpoint
				hasBrick[9 / 2][9 / 2] = false;
				hasForest[9 / 2][9 / 2] = false;
				hasWater[9 / 2][9 / 2] = false;
				hasSteel[9 / 2][9 / 2] = true;

				for (int tank = 0; tank < tankPerSide; tank++) {
					hasSteel[9 / 2][tankX[0][tank]] = true;
					hasForest[9 / 2][tankX[0][tank]] = hasWater[9 / 2][tankX[0][tank]] = hasBrick[9 / 2][tankX[0][tank]] = false;
					for (int y = 0; y < portionH; y++)
						for (int x = 0; x < 9; x++)
							if (hasForest[y][x])hasSteel[y][x] = hasWater[y][x] = hasBrick[y][x] = false;
				}
			} while (!EnsureConnected(hasWater, hasSteel));
			for (int i = 0; i < 3; i++)//3 row as one number
			{
				int mask = 1;
				for (int y = i * 3; y < (i + 1) * 3; y++)
				{
					for (int x = 0; x < 9; x++)
					{
						if (hasBrick[y][x])
							fieldBinary[i] |= mask;
						else if (hasWater[y][x])
							waterBinary[i] |= mask;
						else if (hasSteel[y][x])
							steelBinary[i] |= mask;
						else if (hasForest[y][x])
							forestBinary[i] |= mask;
						mask <<= 1;
					}
				}
			}
		}
		void color_block(bool a[9][9], int posy, int posx)
		{
			if (posy >= 9)return;
			if (posx >= 9)return;
			a[posy][posx] = true;
		}
		// 利用BFS保证连通的EnsureConnected
		// 同样可能成为性能瓶颈	

		//BFS to ensure that there is only one connected component
		//InitializeField already ensures that water&steel will not appear on base and tank
		struct node { int x, y; node() {}node(int xx, int yy) { x = xx, y = yy; } };
		std::queue<node> q;
		bool EnsureConnected(bool hasWater[9][9], bool hasSteel[9][9]) {
			int jishu = 0, jishu2 = 0;
			bool vis[9][9] = { 0 };
			for (int i = 0; i < 9; i++)
				for (int j = 0; j < 9; j++)
					if (!(hasWater[i][j] || hasSteel[i][j]))
						jishu++;
			q.push(node(baseX[0], baseY[0]));
			vis[baseX[0]][baseY[0]] = 1;
			jishu2 = 1;
			while (!q.empty()) {
				int x = q.front().x, y = q.front().y;
				q.pop();
				for (int i = 0; i < 4; i++) {
					int eks = x + dx[i];
					int wai = y + dy[i];
					if (CoordValid(eks, wai) && !vis[eks][wai] && (!(hasWater[eks][wai] || hasSteel[eks][wai]))) {
						q.push(node(eks, wai));
						vis[eks][wai] = 1;
						jishu2++;
					}
				}
			}
			return jishu2 == jishu;
		}


		Judge(bool b_longtime = false, bool r_longtime = false) {
			long_time[blue] = b_longtime;
			long_time[red] = r_longtime;
			Json::Value info;
			do {
				InitializeField();
				Json::Value temp;
				Json::Reader _curr_reader;
				Json::FastWriter _curr_writer;
				const string s = "{\"requests\":[{\"brickfield\":[7369131,57890422,111964272],\"mySide\":0,\"steelfield\":[262144,567424,256],\"waterfield\":[590848,0,66688]}],\"responses\":[]}";
				_curr_reader.parse(s, info);
				for (int i = 0; i < 3; i++)
					temp[i] = fieldBinary[i];
				info["requests"][0u]["brickfield"] = temp;
				for (int i = 0; i < 3; i++)
					temp[i] = waterBinary[i];
				info["requests"][0u]["waterfield"] = temp;
				for (int i = 0; i < 3; i++)
					temp[i] = steelBinary[i];
				info["requests"][0u]["steelfield"] = temp;
				for (int i = 0; i < 3; i++)
					temp[i] = forestBinary[i];
				info["requests"][0u]["forestfield"] = temp;
				field.init(info);
			} while (!field.ValidSymetry());
			output = info;
		}

		
		Judge(Json::Value & info) {
			field.init(info);
			output = info;
		}

		Json::Value judge_main() {
			
				round++;
				Json::Value temp;
				output["requests"][0u]["mySide"] = 0;
				temp.append(output);
				output["requests"][0u]["mySide"] = 1;
				temp.append(output);
				output = temp;
				return output;
			
		}


		Json::Value judge_main(Json::Value & blue_bot, Json::Value & red_bot) {
			if (round == 0) {
				round++;
				Json::Value temp;
				output["request"][0u]["mySide"] = 0;
				temp.append(output);
				output["request"][1u]["mySide"] = 1;
				temp.append(output);
				output = temp;
				return output;
			}
			else {
				round++;
				Json::FastWriter writer;
				int act[4] = {
					blue_bot[0u].asInt(),blue_bot[1].asInt(),
					red_bot[0u].asInt(),red_bot[1].asInt()
				};
				;
				game_result = field.play(act);
				if (long_time[blue]) {
					output[0u] = field.get_request(blue);
				}
				else {
					output[0u]["requests"].append(field.get_request(blue));
					output[0u]["responses"].append(field.get_response(blue));
				}
				if (long_time[red]) {
					output[1] = field.get_request(red);
				}
				else {
					output[1]["requests"].append(field.get_request(red));
					output[1]["responses"].append(field.get_response(red));
				}
				return output;
			}
		}
	};
}



int main()
{
	const int blue = 0, red = 1;
	using namespace std;

	int blue_win = 0, red_win = 0, peace = 0;
	
	// 外层控制对局数
	Json::FastWriter writer;
	for (size_t i = 0; i < 10000; i++)
	{
		//if (i == 358)cout << "DEBUG";
		int game_result = judge::game_continue;
		// 初始化
		// Bot bot1;Bot bot2;
		int seed = time(0);
		srand(seed);
		judge::Judge JJ(true, true);
		Json::Value output = JJ.judge_main();
		int turn = 0;
		clock_t time = clock();
		AlphaTankZero::Bot bot1(true); AlphaTankZero1::Bot bot2(true);
		while(game_result == judge::game_continue && turn<100) {

			//if (turn == 35)	cout << " ";
			//cout << writer.write(output);
			// toBlue 和 toRed就是发给红蓝双方的行动
			// 用 outBlue 和 outRed来接收输出;
			Json::Value toBlue = output[0u];
			Json::Value toRed = output[1];

			//cout << "轮次 "<<turn<<endl;
			//cout << "蓝方\n";
			//cout << writer.write(toBlue);
			//cout << "红方\n";
			//cout << writer.write(toRed);
			Json::Value outBlue = bot1.bot_main(toBlue);
			Json::Value outRed = bot2.bot_main(toRed);

		
			if (!(bot1.round[turn] == JJ.field.gamefield))
				system("pause");
			if (!(bot2.round[turn] == JJ.field.gamefield))
				system("pause");
			for (size_t i = 0; i < 2; i++)
			{
				bot1.round[turn].valid_self_pos(i, JJ.field.tank[blue][i].x, JJ.field.tank[blue][i].y);
				bot1.round[turn].valid_enemy_pos(i, JJ.field.tank[red][i].x, JJ.field.tank[red][i].y);
				
				bot2.round[turn].valid_self_pos(i, JJ.field.tank[red][i].x, JJ.field.tank[red][i].y);
;				bot2.round[turn].valid_enemy_pos(i, JJ.field.tank[blue][i].x, JJ.field.tank[blue][i].y);
			}
			// 完全对称局 局面对称性检查开关
			// if (!JJ.field.ValidSymetry())
			//	cout << "局面不对称\n";

			turn++;
			output = JJ.judge_main(outBlue,outRed);
			game_result = JJ.game_result;
			switch (game_result)
			{
			case judge::blue:
				//蓝方胜利
				blue_win++;
				printf( "蓝方胜利\n");
				break;
			case judge::red:
				//红方胜利
				red_win++;
				printf("红方胜利\n");
				break;
			case judge::nowinner:
				//平局
				peace++;
				printf( "平局\n");
				break;
			default:
				//继续
				
				break;
			}
		}
		if (turn >= 100&&game_result==judge::game_continue)peace++;
		printf("顺利完成第%d局，共%d轮，耗时%dms\n", 
			i + 1, turn, clock() - time);
	}
	cout << "10000局完成，总耗时" << clock() << "ms\n";
	printf("蓝方胜场：%d\t红方胜场：%d\t平局：%d\n", blue_win, red_win, peace);
	printf("蓝方射击次数：%d\t移动次数：%d\t静止次数：%d\t坦克阵亡次数：%d\n",
		AlphaTankZero::shoot_cnt, AlphaTankZero::move_cnt, AlphaTankZero::stay_cnt,judge::blue_tank_die);
	printf("红方射击次数：%d\t移动次数：%d\t静止次数：%d\t坦克阵亡次数：%d\n",
		AlphaTankZero1::shoot_cnt, AlphaTankZero1::move_cnt, AlphaTankZero1::stay_cnt,judge::red_tank_die);
	system("pause");
	
	return 0;
}
