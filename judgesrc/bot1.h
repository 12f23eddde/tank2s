#define _CRT_SECURE_NO_WARNINGS 1
//#define _BOTZONE_ONLINE

#include<vector>
#include<queue>
#include<map>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include "jsoncpp/json.h"
#include<algorithm>

namespace AlphaTankZero1 {
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
	// 比较局面是否相等的debug函数


	// 全局
	int self, enemy;
	// Debug用统计变量
	int shoot_cnt = 0;
	int move_cnt = 0;
	int stay_cnt = 0;
	struct Pos {
		int x, y;
		bool operator<(const Pos&p) {
			return x * 9 + y<p.x * 9 + p.y;
		}
	};

	struct Action {
		int a[2];
	};


	struct Tank {
		int x, y;
		int action = -1;
		int steps_in_forest = 0;
		bool alive = true;
		inline void setpos(int _x, int _y) {
			x = _x, y = _y;
			if (x != -2)steps_in_forest = 0;
		}
		inline void setact(int act) { action = act; }
		inline void die() {
			alive = false;
			x = -1, y = -1;
		}
		inline bool can_shoot()const {
			return action < 4;
		}
		inline bool in_forest() { return  x == -2; }
		inline bool at(int _x, int _y)const { return x == _x && y == _y; }
		int & operator[](int index) { if (index == 0)return x; else return y; }
		void move(int act) {
			if (!alive)return;
			action = act;
			if (act == -1)return;
			x += dx[act];
			y += dy[act];
		}
		void shoot(int act) { action = act; }
		bool operator==(const Tank & t)const { return x == t.x&&y == t.y; }
	};


	struct Battlefield {

		int gamefield[9][9];//main_field
		static bool possible_field[2][9][9];//possible enemy position in forest
		int steps = 0;
		int value = 0;
		static int stay;
		Tank tank[2][2];


		//用于检查possible_field的debug函数
		bool valid_enemy_pos(int id, int x, int y) {
			if (!tank[enemy][id].alive)return true;
			if (gamefield[y][x] != forest) {
				if (tank[enemy][id].at(x, y))	return true;
				else return false;
			}
			if (possible_field[id][y][x]) {
				return true;
			}
			else {
				return false;
			}
		}
		bool valid_self_pos(int id, int x, int y) {
			if (!tank[self][id].alive)return true;
			if (tank[self][id].at(x, y))return true;
			else return false;

		}



		bool operator==(int field[9][9]) {
			for (size_t i = 0; i < 9; i++)
			{
				for (size_t j = 0; j < 9; j++)
				{
					if (gamefield[i][j] != field[i][j]) {
						return false;
					}
				}
			}
			return true;

		}

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


		inline bool has_barrier(int x, int y)const {
			return gamefield[y][x] == brick || gamefield[y][x] == steel || gamefield[y][x] == base || has_tank(x, y);
		}

		int cnt_tank(int x, int y)const {
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

		Battlefield(const Battlefield &b) {
			*this = b;
		}
		~Battlefield() { }

		bool operator<(const Battlefield & b) { return value < b.value; }
		void init(Json::Value & info) {
			memset(gamefield, 0, 81 * sizeof(int));
			memset(possible_field, false, sizeof(possible_field));
			gamefield[0][4] = gamefield[8][4] = base;
			Json::Value requests = info["requests"], responses = info["responses"];
			//assert(requests.size());
			//load battlefield info
			self = requests[0u]["mySide"].asInt();
			enemy = 1 - self;
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
		bool lose(bool enemy = false) {
			if (gamefield[0][4] == none && gamefield[8][4] == none) {
				return false;
			}
			int nowside = self;
			if (enemy)nowside = 1 - nowside;
			if (gamefield[0][4] == none && nowside == 0) return true;
			if (gamefield[8][4] == none && nowside == 1) return true;
			return false;
		}

		// 判断当前位置是否会被攻击

		bool in_danger(int side, int id)const {
			if (!tank[side][id].alive) return false;
			if (!tank[side][0].can_shoot() && !tank[side][1].can_shoot())return false;
			if (can_attack_base(side, id))return false;
			for (size_t i = 0; i < 2; i++)
			{
				if (tank[1 - side][i].alive && tank[1 - side][i].can_shoot() &&
					can_shoot(*this, tank[1 - side][i].x, tank[i - side][i].y, tank[side][id].x, tank[side][id].y))
					return true;
			}
			return false;
		}


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
					if (!tank[1 - side][j].alive)continue;
					if (tank[1 - side][j].x == neibor[i * 2] && tank[1 - side][j].y == neibor[i * 2 + 1])return true;
					if (side == self && possible_field[j][neibor[i * 2 + 1]][neibor[i * 2]] == true)return true;
				}
			}
			return false;
		}


		// 判断坐标x，y是不是自己射的
		// 如果是，那么清空该路径上所有的可能森林

		bool shoot_by(int side, int x, int y)const {
			for (size_t i = 0; i < 2; i++)
			{
				bool cannot_shoot = false;
				if (!tank[side][i].alive)continue;
				if (tank[side][i].at(x, y))continue;
				int act = tank[side][i].action;
				if (act > 3) {
					if (x == tank[side][i].x &&
						((act == 4 && tank[side][i].y > y) ||
						(act == 6 && tank[side][i].y < y))) {
						int tmp_y = tank[side][i].y + dy[act - 4];
						while (tmp_y != y) {
							if (has_barrier(x, tmp_y)) {
								cannot_shoot = true;
							}
							tmp_y += dy[act - 4];
						}
						if (cannot_shoot)continue;
						// 射击命中
						// 如果射击者是己方，顺便清理可能森林

						return true;
					}
					else if (y == tank[side][i].y &&
						((act == 5 && tank[side][i].x < x) ||
						(act == 7 && tank[side][i].x > x))) {
						int tmp_x = tank[side][i].x + dx[act - 4];
						while (tmp_x != x) {
							if (has_barrier(tmp_x, y)) {
								cannot_shoot = true;
							}
							tmp_x += dx[act - 4];
						}
						if (cannot_shoot)continue;

						return true;
					}
				}
			}
			return false;
		}
		//when enemy in forest destroyed something, just find the shooter.
		void set_forest_shooter(int x, int y) {

			bool tmp_field[2][9][9];
			memset(tmp_field, 0, 162 * sizeof(bool));
			bool possible[2] = { false,false };
			int unique_shoot = 0;
			if (tank[enemy][0].steps_in_forest == 0 &&
				tank[enemy][1].steps_in_forest == 0) return;
			for (size_t j = 0; j < 2; j++)
			{
				if (tank[enemy][j].steps_in_forest == 0)continue;
				for (int m = y - 1; m >= 0; m--)
				{
					if (gamefield[m][x] == none || gamefield[m][x] == water)continue;
					if (gamefield[m][x] == brick || gamefield[m][x] == steel || gamefield[m][x] == base)break;
					if (unique_shoot == 0)unique_shoot = 6;
					else if (unique_shoot != 6)unique_shoot = -1;
					if (possible_field[j][m][x] == true) {
						possible[j] = true;
						tmp_field[j][m][x] = possible_field[j][m][x];
						if (possible_field[1 - j][m][x])return;
					}
					if (has_tank(x, m))break;
				}
				for (size_t i = y + 1; i < 9; i++)
				{
					if (gamefield[i][x] == none || gamefield[i][x] == water)continue;
					if (gamefield[i][x] == brick || gamefield[i][x] == steel || gamefield[i][x] == base)break;
					if (possible_field[j][i][x] == true) {
						if (unique_shoot == 0)unique_shoot = 4;
						else if (unique_shoot != 4)unique_shoot = -1;
						possible[j] = true;
						tmp_field[j][i][x] = possible_field[j][i][x];
						if (possible_field[1 - j][i][x])return;
					}
					if (has_tank(x, i))break;
				}
				for (int i = x + 1; i < 9; i++)
				{
					if (gamefield[y][i] == none || gamefield[y][i] == water)continue;
					if (gamefield[y][i] == brick || gamefield[y][i] == steel || gamefield[y][i] == base)break;
					if (possible_field[j][y][i] == true) {
						if (unique_shoot == 0)unique_shoot = 7;
						else if (unique_shoot != 7)unique_shoot = -1;
						possible[j] = true;
						tmp_field[j][y][i] = possible_field[j][y][i];
						if (possible_field[1 - j][y][i])return;
					}
					if (has_tank(i, y))break;
				}
				for (int i = x - 1; i >= 0; i--)
				{
					if (gamefield[y][i] == none || gamefield[y][i] == water)continue;
					if (gamefield[y][i] == brick || gamefield[y][i] == steel || gamefield[y][i] == base)break;
					if (possible_field[j][y][i] == true) {
						if (unique_shoot == 0)unique_shoot = 5;
						else if (unique_shoot != 5) unique_shoot = -1;
						possible[j] = true;
						tmp_field[j][y][i] = possible_field[j][y][i];
						if (possible_field[1 - j][y][i])return;
					}
					if (has_tank(i, y))break;
				}
			}
			if (possible[0] && possible[1])return;
			if (possible[0])memcpy(possible_field[0], tmp_field[0], sizeof(bool) * 81);
			if (possible[1])memcpy(possible_field[1], tmp_field[1], sizeof(bool) * 81);
			if (unique_shoot != -1) {
				if (possible[0])tank[enemy][0].shoot(unique_shoot);
				if (possible[1])tank[enemy][1].shoot(unique_shoot);
			}

			return;
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
				if (actionlist[id] >= 4)continue;
				tank[enemy][id].move(actionlist[id + 2]);

			}
			vector<Pos> destroylist;
			bool tank_die[2][2] = { false,false,false,false };
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
						if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
							break;
						if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
							destroylist.push_back(Pos{ xx, yy });
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
								if (tank_cnt > 1)tank_die[dst_side][dst_id] = true;
								if (tank_cnt == 1) {
									if (actionlist[2 * side + id] != opposite_shoot(actionlist[2 * dst_side + dst_id])) {
										tank_die[dst_side][dst_id] = true;
									}
								}
							}
						}
						break;
					}
				}
			}
			// 结算射击结果
			for (auto & i : destroylist)gamefield[i.y][i.x] = none;
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank_die[side][id]) {
						tank[side][id].die();
						if (side == enemy)memset(possible_field[id], 0, 81 * sizeof(bool));
					}
				}
			}

		}


		// 验证移动是否合法
		bool move_valid(int side, int id, int act)const
		{
			if (act == -1)return true;
			if (!tank[side][id].alive && act != -1)return false;
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

		// 射击粗剪枝
		// 射击没有摧毁物品、打不到敌人且没有危险性的都剪掉。

		bool shoot_valid(int side, int id, int act) const {

			if (!tank[side][id].can_shoot())return false;
			if (!tank[side][id].alive && act != -1)return false;

			int xx = tank[side][id].x;
			int yy = tank[side][id].y;
			//bool shoot_enemy = false;
			while (true) {
				xx += dx[act - 4];
				yy += dy[act - 4];
				if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel) {
					return true;//shoot_enemy;
				}
				//shoot_enemy |= possible_enemy(side, xx, yy, act - 4);
				if (gamefield[yy][xx] == base) {
					if (yy == (side == blue ? 0 : 8))
						return false;
					else return true;
				}
				if (gamefield[yy][xx] == brick) {
					return true;
				}
			}

			return true;
		}


		bool can_shoot(const Battlefield & f, int x, int y, int dst_x, int dst_y) const {
			if (x != dst_x && y != dst_y)return false;
			if (x == dst_x) {
				int y_step = y < dst_y ? 1 : -1;
				for (size_t i = y + y_step; i != dst_y; i += y_step)
				{
					if (has_barrier(x, i))return false;
				}
			}
			else {
				int x_step = x < dst_x ? 1 : -1;
				for (size_t i = x + x_step; i != dst_x; i += x_step)
				{
					if (has_barrier(i, y))return false;
				}
			}
			return true;
		}
		//locate_enemy using two-method : attack us or attack base
		void locate_enemy(vector<Battlefield> & cases)const {

			Battlefield tmp = *this;
			vector<Pos> enemypos[2];
			for (size_t id = 0; id < 2; id++)
			{
				if (tmp.tank[enemy][id].steps_in_forest != 0) {
					for (int i = 0; i < 9; i++)
					{
						for (int j = 0; j < 9; j++)
						{
							if (possible_field[id][i][j] == true) {
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
					Battlefield current = tmp;
					tmp.tank[enemy][0].x = enemypos[0][i].x;
					tmp.tank[enemy][0].y = enemypos[0][i].y;
					tmp.tank[enemy][1].x = enemypos[1][j].x;
					tmp.tank[enemy][1].y = enemypos[1][j].y;
					current.eval_enemy_pos();
					cases.push_back(current);
				}
			}
		}

		bool can_attack_base(int side, int id)const {
			if (!tank[side][id].alive)return false;

			int x = 4, y = (self == red && side == enemy) || (self == blue && side == self) ? 8 : 0;
			return can_attack(side, id, x, y);
		}
		inline bool can_attack(int side, int id, int x, int y)const {
			return can_shoot(*this, tank[side][id].x, tank[side][id].y, x, y);
		}

		inline int cost_attack_base(int id, bool enemy = false)const {
			int x = 4, y = (self&&enemy) || (!self && !enemy) ? 8 : 0;
			return cost_attack(id, enemy, x, y);
		}

		int cost_attack(int side, int id, int x, int y)const {
			struct Block {
				int x, y;
				int cost;
				int estimate_cost;
				Block(int xx, int yy, int _cost) :x(xx), y(yy), cost(_cost), estimate_cost(0) {}
				bool operator<(const Block & b)const { return cost + estimate_cost > b.cost + b.estimate_cost; }
				inline bool valid() { return x >= 0 && x < 9 && y >= 0 && y < 9; }
			};
			Battlefield tmp = *this;
			priority_queue<Block> list;
			bool visited[9][9] = { false };
			Block now(tank[side][id].x, tank[side][id].y, 0);
			if (can_shoot(tmp, now.x, now.y, x, y))
				return 1;
			list.push(now);
			while (!list.empty()) {
				Block center = list.top();
				list.pop();
				Block neibor[4] = {
					{ center.x + 1, center.y,center.cost + 1 },
				{ center.x - 1, center.y,center.cost + 1 },
				{ center.x, center.y + 1,center.cost + 1 },
				{ center.x, center.y - 1,center.cost + 1 }
				};
				for (size_t i = 0; i < 4; i++)
				{
					if (neibor[i].valid() && gamefield[neibor[i].y][neibor[i].x] != steel && gamefield[neibor[i].y][neibor[i].x] != water) {
						if (visited[neibor[i].y][neibor[i].x])continue;
						visited[neibor[i].y][neibor[i].x] = true;
						if (tmp.gamefield[neibor[i].y][neibor[i].x] == brick) {
							neibor[i].cost++;
						}
						if (can_shoot(tmp, neibor[i].x, neibor[i].y, x, y)) {
							return neibor[i].cost + 1;
						}
						neibor[i].estimate_cost = abs(neibor[i].x - x) + abs(neibor[i].y - y);
						list.push(neibor[i]);
					}
				}
			}
			return INF;
		}


		int speed_attack_base() const {
			int result = 0;
			for (size_t i = 0; i < 2; i++)
			{
				if (tank[self][i].alive) {
					result += 30 - cost_attack_base(i);
				}
			}
			return result;
		}

		void eval_enemy_pos() {
			int attack1, attack2;
			int cost[4];
			attack1 = cost_attack(0, true, tank[self][0].x, tank[self][0].y);
			attack2 = cost_attack(0, true, tank[self][1].x, tank[self][1].y);
			cost[0] = attack1 < attack2 ? attack1 : attack2;
			attack1 = cost_attack(1, true, tank[self][0].x, tank[self][0].y);
			attack2 = cost_attack(1, true, tank[self][1].x, tank[self][1].y);
			cost[1] = attack1 < attack2 ? attack1 : attack2;
			cost[0] += 5; cost[1] += 5;
			cost[2] = cost_attack_base(0, true);
			cost[3] = cost_attack_base(1, true);
			if (cost[0] > cost[2])cost[0] *= 0.4;
			else cost[2] *= 0.4;
			if (cost[1] > cost[3])cost[1] *= 0.5;
			else cost[3] *= 0.5;
			for (size_t i = 0; i < 4; i++)
			{
				value -= cost[i];
			}
		}

		int eval_field() {
			if (gamefield[0][4] == none && gamefield[8][4] == none)return 0;
			if (gamefield[8][4] == none) {
				if (self == 0)
					value += 2000;
				else value -= 1000;
			}
			else if (gamefield[0][4] == none) {
				if (self == 0)
					value -= 1000;
				else value += 2000;
			}
			for (size_t i = 0; i < 2; i++) {
				if (tank[enemy][i].alive == false)value += 300;
				if (tank[self][i].alive == false)value -= 500;
			}

			for (size_t i = 0; i < 2; i++)
			{
				if (tank[self][i].alive)value += 50 - cost_attack_base(i)*0.5;
				if (tank[enemy][i].alive)value -= 50 - cost_attack_base(i, true);
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
		void Update(Battlefield & f, Json::Value & request, int * responses) {
			// 局面更新函数
			// 先记录再销毁以保证本地局面与裁判完全一致
			vector<Pos> distroyed_blocks;
			bool tank_die[2][2] = { 0 };
			f.steps++;
			for (int j = 0; j < 2; j++)
			{
				if (!f.tank[self][j].alive) {
					continue;
				};
				int x = responses[j];
				if (x >= 4)f.tank[self][j].shoot(x);
				else {
					if (x == -1)f.stay++;
					else f.stay = 0;
					f.tank[self][j].move(x);
				}
			}
			//update enenmy location and action
			for (size_t j = 0; j < 2; j++)
			{
				int last_act = request["action"][j].asInt();

				if (!f.tank[enemy][j].alive)continue;
				int last_x = f.tank[enemy][j].x;
				int last_y = f.tank[enemy][j].y;
#ifdef DEBUG
				if (last_x == -2 && last_act != -2) {
					cout << "DEBUG信息：裁判输入不合法\n";
					cout << "Tank id: " << j << "  Action: " << last_act << endl;
				}

#endif // DEBUG

				int now_x = request["final_enemy_positions"][2 * j].asInt();
				int now_y = request["final_enemy_positions"][2 * j + 1].asInt();
				f.tank[enemy][j].setpos(now_x, now_y);
				f.tank[enemy][j].setact(last_act);
				if (now_x == -1) {
					tank_die[enemy][j] = true;
					continue;
				}

				if (f.tank[enemy][j].in_forest()) {
					if (is_move(last_act)) {
						// 进入森林
						int x = last_x + dx[last_act],
							y = last_y + dy[last_act];
						f.possible_field[j][y][x] = true;
					}
					else {
						// 在森林中
						f.tank[enemy][j].steps_in_forest++;
					}
				}
				else {
					if (last_act == -2) {
						// 敌方走出森林
						memset(f.possible_field[j], false, sizeof(bool) * 81);
						// 反推上回合行动
						int x = last_x, y = last_y;
						// 先横后纵，第一个有可能蹲的草就是敌方曾经的位置。
						// 由于草丛边缘形状简单，通常不会判断不准。
						int ny[2] = { y - 1 >= 0 ? y : 0,y + 1 <= 8 ? y + 1 : 8 };
						int nx[2] = { x - 1 >= 0 ? x - 1 : 0,x <= 8 ? x + 1 : 8 };
						if (f.possible_field[j][y][nx[0]] == true)last_act = 1;
						else if (f.possible_field[j][y][nx[1]] == true)last_act = 3;
						else if (f.possible_field[j][ny[0]][x] == true)last_act = 2;
						else if (f.possible_field[j][ny[1]][x] == true)last_act = 0;
					}
					// 此时敌方一定在森林外，开枪与否要么为-2，要么被推断出来
					f.tank[enemy][j].setact(last_act);
				}
				// 此时单凭行动和坐标的信息都已判断完毕了。
			}
			for (size_t j = 0; j < request["destroyed_tanks"].size(); j += 2)
			{
				int x = request["destroyed_tanks"][j].asInt(),
					y = request["destroyed_tanks"][j + 1].asInt();
				// 己方重叠的坦克不可能被一起打死
				if (f.tank[self][0].at(x, y)) {
					tank_die[self][0] = true;
					continue;
				}
				if (f.tank[self][1].at(x, y)) {
					tank_die[self][1] = true;
					continue;
				}
			}
			// 此时敌方死亡坦克状态已还原
			// 可以判断敌方是否射击

			for (size_t j = 0; j < request["destroyed_tanks"].size(); j += 2)
			{
				int x = request["destroyed_tanks"][j].asInt(),
					y = request["destroyed_tanks"][j + 1].asInt();
				// 己方重叠的坦克不可能被一起打死
				// 为简化工作，只在敌方坦克未死时推断位置
				if (!tank_die[enemy][0] && !tank_die[enemy][1]) {
					if (!f.shoot_by(self, x, y) && !f.shoot_by(enemy, x, y)) {
						// 我方坦克已死
						// 根据我方坦克死亡位置 判断敌方在森林中的位置
						f.set_forest_shooter(x, y);
					}
				}
			}

			for (size_t j = 0; j < request["destroyed_blocks"].size(); j += 2)
			{
				int x = request["destroyed_blocks"][j].asInt();
				int y = request["destroyed_blocks"][j + 1].asInt();
				distroyed_blocks.push_back(Pos{ x,y });
				// 若是敌方坦克死亡，那么仅知道对方死亡坦克位置
				// 为简化工作，只在敌方坦克未死时推断位置
				if (!tank_die[enemy][0] && !tank_die[enemy][1]) {
					if (!f.shoot_by(self, x, y) && !f.shoot_by(enemy, x, y)) {
						f.set_forest_shooter(x, y);
					}
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
								if (f.possible_field[j][m][n] == true && f.gamefield[m][n] == forest) {
									tmp_possible_field[m][n] = true;
									int y[2] = { m - 1 >= 0 ? m - 1 : 0,m + 1 <= 8 ? m + 1 : 8 };
									int x[2] = { n - 1 >= 0 ? n - 1 : 0,n + 1 <= 8 ? n + 1 : 8 };
									tmp_possible_field[y[0]][n] |= (f.gamefield[y[0]][n] == forest);
									tmp_possible_field[y[1]][n] |= (f.gamefield[y[1]][n] == forest);
									tmp_possible_field[m][x[0]] |= (f.gamefield[m][x[0]] == forest);
									tmp_possible_field[m][x[1]] |= (f.gamefield[m][x[1]] == forest);
								}
							}
						}

						memcpy(f.possible_field[j], tmp_possible_field, sizeof(tmp_possible_field));
					}
				}

			}
			// 死亡结算
			for (auto & i : distroyed_blocks) {
				f.gamefield[i.y][i.x] = none;
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (tank_die[self][id])
					f.tank[self][id].die();
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (tank_die[enemy][id]) {
					f.tank[enemy][id].die();
					memset(f.possible_field[id], false, 81 * sizeof(bool));
				}
			}
		}




		Json::Value bot_main(Json::Value input)
		{
			Json::Reader reader;
			Json::Value temp, output;
			output = Json::Value(Json::arrayValue);

			if (!long_time) {

				round.push_back(Battlefield());
				round[0].init(input);
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
				if (round.size() == 0) {
					round.push_back(Battlefield());
					round[0].init(input);
				}
				else {
					Battlefield tmp = round[round.size() - 1];
					Update(tmp, input, last_act);
					round.push_back(tmp);
				}
			}
			Battlefield *pt = &(round[round.size() - 1]);
			vector<int> act[2];

			for (size_t id = 0; id < 2; id++)
			{
				for (size_t i = 0; i < 2; i++)
				{
					if (pt->move_valid(self, id, i))act[id].push_back(i);
				}
				for (size_t i = 2; i < 4; i++)
				{
					if (pt->move_valid(self, id, i))act[id].push_back(i);
				}
				for (size_t i = 4; i < 6; i++)
				{
					if (pt->shoot_valid(self, id, i))
						act[id].push_back(i);
				}
				for (size_t i = 6; i < 8; i++)
				{
					if (pt->shoot_valid(self, id, i))
						act[id].push_back(i);
				}
			}
			if (act[0].size() == 0)act[0].push_back(-1);
			if (act[1].size() == 0)act[1].push_back(-1);
			// 全对称测试开关
			// srand(round.size());
			Action max;
			max.a[0] = act[0][rand() % act[0].size()];
			max.a[1] = act[1][rand() % act[1].size()];
			if (long_time) {
				last_act[0] = max.a[0];
				last_act[1] = max.a[1];
			}

			output.append(max.a[0]);
			output.append(max.a[1]);
			if (max.a[0] >= 4)shoot_cnt++;
			else if (max.a[0] == -1)stay_cnt++;
			else move_cnt++;
			if (max.a[0] >= 4)shoot_cnt++;
			else if (max.a[0] == -1)stay_cnt++;
			else move_cnt++;

			return output;
		}
	};
}

#define DEBUG

#ifdef _BOTZONE_ONLINE

int main()
{
	using namespace AlphaTankZero;
	srand(time(0));
	Json::Reader reader;
	Json::Value input, output;
	Json::FastWriter writer;
#ifndef _BOTZONE_ONLINE
	string s = string("{\"requests\":[{\"brickfield\":[105148584,4937360,11038995],\"forestfield\":[28871682,61866094,33655020],\"mySide\":1,\"steelfield\":[0,43008,0],\"waterfield\":[0,0,0]},{\"action\":[2,2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,-2,-2]}],\"responses\":[[0,0]]}");

	reader.parse(s, input);
#else
	string s;
	getline(cin, s);
	reader.parse(s, input);
#endif


	Bot bot(true);
	output = bot.bot_main(input);
	cout << writer.write(output);
	while (bot.longtime()) {
		cout << "\n>>>BOTZONE_REQUEST_KEEP_RUNNING<<<\n";
		cout << flush;
		getline(cin, s);
		reader.parse(s, input);
		output = bot.bot_main(input);
		cout << writer.write(output)
	}


#ifndef _BOTZONE_ONLINE
	cout << clock();
	system("pause");
#endif
	return 0;
}
#endif