// Tank2S 本地裁判程序
// 根据Bot修改而来
// 由于无需与平台交互，非常精简
// 且裁判默认为长时运行


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
	inline int opposite_shoot(int act) { return act > 5 ? act - 2 : act + 2; }

	// 全局
	int self = blue, enemy = red;
	bool vague_death = false;

	struct Pos {
		int x, y;
		bool operator==(const Pos & p)const { return x == p.x&&y == p.y; }
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
		inline void die(Battlefield & f) { 
			action = -1;
			alive = false;
			if (in_forest(f)) {
				x = -2, y = -2; 
			}
			else {
				x = -1, y = -1;
			}
		}
		inline bool alive() const { return alive; }
		inline bool can_shoot()const {
			return action < 4;
		}
		inline bool in_forest(Battlefield & f) { return f.gamefield[y][x]==forest; }
		inline bool at(int _x, int _y)const { return x == _x && y == _y; }
		int & operator[](int index) { if (index == 0)return x; else return y; }
		void move(int act) {
			if (act == -1)return;
			x += dx[act];
			y += dy[act];
			action = act;
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
		vector<Tank> prev_tanks[2];
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

		bool operator<(const Battlefield & b) { return value < b.value; }
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
			result.append(prev_tanks[side][0].action);
			result.append(prev_tanks[side][1].action);
			return result;
		}


		Json::Value get_request(int side) {
			int enemy_act[2] = {
				prev_tanks[1 - side][0].in_forest(*this) ? -2:tank[1 - side][0].action,
				prev_tanks[1 - side][1].in_forest(*this) ? -2:tank[1 - side][1].action
			};
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
				int enemypos[2] = { tank[1 - side][i].x ,tank[1 - side][i].y };
				if (tank[1 - side][i].in_forest(*this)) {
					request["destroyed_tanks"].append(-2);
					request["destroyed_tanks"].append(-2);
				}
				else {
					request["final_enemy_positions"].append(enemypos[0]);
					request["final_enemy_positions"].append(enemypos[1]);
				}

			}

		}


		void debug_print() {

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
			set<Pos> distroylist;
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
						if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
							break;
						if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
							distroylist.insert(Pos{ xx, yy });
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
			for (size_t id = 0; id < 2; id++)
			{
				if (actionlist[id] >= 4) {
					tank[self][id].shoot(actionlist[id]);
					int xx = tank[self][id].x;
					int yy = tank[self][id].y;
					while (true) {
						xx += dx[actionlist[id] - 4];
						yy += dy[actionlist[id] - 4];
						if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
							break;
						if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
							distroylist.insert(Pos{ xx, yy });
							break;
						}
						for (size_t i = 0; i < 2; i++)
						{
							if (tank[enemy][i].at(xx, yy)) {
								if (tank[enemy][1 - i] == tank[enemy][i]) {
									// 重叠坦克被射击立刻全炸
									tank_die[self][0] = true;
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
			destroyed_tanks.clear();
			destroyed_blocks.clear();
			// 结算射击结果
			for (auto & i : distroylist) {
				gamefield[i.y][i.x] = none;
				destroyed_blocks.push_back(i);
			}
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank_die[side][id]) {
						destroyed_tanks.push_back(tank[side][id]);
						prev_tanks[side][id].action = tank[side][id].action;
						tank[side][id].die(*this);
					}
				}
			}

		}

		int play(int * actionlist) {
			for (size_t side = 0; side < 2; side++)
			{
				prev_tanks[side].clear();
				prev_tanks[side].push_back(tank[side][0]);
				prev_tanks[side].push_back(tank[side][1]);
			}
			bool lose[2] = { false,false };
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(blue, id, actionlist[id]))
				{
					lose[blue] = false;
					cout << "DEBUG信息：蓝方行为不合法\n";
					cout << "Tank id: " << id << "  Action: " << actionlist[id] << endl;
				}
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(red, id, actionlist[id + 2]))
				{
					lose[red] = false;
					cout << "DEBUG信息：红方行为不合法\n";
					cout << "Tank id: " << id << "  Action: " << actionlist[id + 2] << endl;
				}
			}
			if (lose[blue] && lose[red])return nowinner;
			if (lose[red])return blue;
			if (lose[blue])return red;
			quick_play(actionlist);
			bool baseAlive[2] = { gamefield[0][4] != none,gamefield[8][4] != none };
			bool tankAlive[2] = { tank[blue][0].alive() || tank[blue][1].alive(),
				tank[red][0].alive() || tank[red][1].alive() };
			lose[blue] = !baseAlive[blue] || !tankAlive[blue];
			lose[red] = !baseAlive[red] || !tankAlive[red];
			if (lose[blue] && lose[red])return nowinner;
			if (lose[red])return blue;
			if (lose[blue])return red;
			return game_continue;
		}

		// 验证移动是否合法
		bool move_valid(int side, int id, int act)const
		{
			if (act == -1)return true;
			if (act > 3)return true;
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


		// 局面数组 这些数组在initfield中被一一设置
		// 每一位都被设置，故无需初始化
		int fieldBinary[3];
		int waterBinary[3];
		int steelBinary[3];
		int	forestBinary[3];
		// 标准局面生成器
		// 这个辣鸡生成器用了很多全局变量
		// 另外他的rand()很慢 可能成为性能瓶颈

		void InitializeField()
		{
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
			InitializeField();
			Json::Value info, temp;
			Json::Reader _curr_reader;
			Json::FastWriter _curr_writer;
			const string s = "{\"requests\":[{\"brickfield\":[0,0,0],\"mySide\":0,\"steelfield\":[0,0,0],\"waterfield\":[0,0,0]}],\"responses\":[]}";
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
			output = info;

		}
		Judge(Json::Value & info) {
			field.init(info);
			output = info;
		}
		Json::Value judge_main() {
			if (round == 0) {
				round++;
				Json::Value temp;
				output["request"][0u]["mySide"] = 0;
				temp.append(output);
				output["request"][0u]["mySide"] = 1;
				temp.append(output);
				output = temp;
				return output;
			}
		}


		Json::Value judge_main(Json::Value & bot_blue, Json::Value & red_bot) {
			if (round == 0) {
				round++;
				Json::Value temp;
				output["request"][0u]["mySide"] = 0;
				temp.append(output);
				output["request"][0u]["mySide"] = 1;
				temp.append(output);
				output = temp;
				return output;
			}
			else {
				int act[4] = {
					bot_blue[0u].asInt(),bot_blue[1].asInt(),
					red_bot[0u].asInt(),red_bot[1].asInt()
				};
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
					output[1]["requests"].append(field.get_response(red));
					output[1]["responses"].append(field.get_response(red));
				}
				return output;
			}
		}
	};
}



int main()
{
	int game_result;
	// 外层控制对局数
	for (size_t i = 0; i < 100; i++)
	{
		// 初始化
		// Bot bot1;Bot bot2;
		judge::Judge JJ(false, false);
		Json::Value output = JJ.judge_main();
		while(game_result == judge::game_continue) {
			Json::Value toBlue = output[0u];
			Json::Value toRed = output[1];
			// toBlue 和 toRed就是发给红蓝双方的行动
			// 用 outBlue 和 outRed来接收输出;
			Json::Value outBlue; 
			Json::Value outRed; 

			
			output = JJ.judge_main(outBlue,outRed);
			game_result = JJ.game_result;
			switch (game_result)
			{
			case judge::blue:
				//蓝方胜利
				break;
			case judge::red:
				//红方胜利
				break;
			case judge::nowinner:
				//平局
				break;
			default:
				//继续
				break;
			}
		}
	}

	
	return 0;
}
