#define _CRT_SECURE_NO_WARNINGS 1

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

using namespace std;
struct Battlefield {

	struct Action {
		int a[2];
		double value = 0;
		Battlefield * stem_field;
		vector<Battlefield> leaf_field;
		Action(int a0 = -1, int a1 = -1) { a[0] = a0, a[1] = a1; }
		bool operator<(const Action & b)const { return value < b.value; }
		void eval_action() {
			for (auto & i : leaf_field)
			{
				for (size_t j = 0; j < 2; j++)
				{
					i.eval_field();
					if (!i.tankAlive[j] && stem_field->tankAlive[j]) {
						value -= 300;
					}
				}
				value += i.value*i.weight;
			}
		}
	};
	static Action root_action;
	vector<Action> leaf_action;
	Action * stem_action;//last action
	static constexpr int dx[4] = { 0,1,0,-1 };
	static constexpr int dy[4] = { -1,0,1,0 };
	static constexpr int none = 0, brick = 1, steel = 2, water = 4, forest = 8, base = 16;
	int gamefield[9][9];//main_field
	bool possible_field[2][9][9];//possible enemy position in forest
	int self_position[4];
	int enemy_position[4];
	int e_stem_action[2] = { -1,-1 };
	int myside;
	bool tankAlive[2] = { true,true };
	bool e_tankAlive[2] = { true,true };
	bool shoot[2] = { false,false };//shoot or not?
	bool e_shoot[2] = { false,false };//enemy shoot or not?
	int steps_in_forest[2] = { 0,0 };
	double value = 0;
	double weight = 1;

	static inline bool isMove(int action) { return action >= 0 && action <= 3; }
	static inline bool isShoot(int action) { return action >= 4; }
	static inline int opposite_shoot(int act) { return act > 5 ? act - 2 : act + 2; }
	static inline int abs(int a) { return a > 0 ? a : -a; }


	
	void valid_possiblefield() {
		if (enemy_position[0] != -2 && enemy_position[2] != -2)return;
		
		for (size_t id = 0; id < 2; id++)
		{
			bool result = false;
			if (enemy_position[2*id]==-2)
			{
				for (size_t i = 0; i < 9; i++)
				{
					for (size_t j = 0; j < 9; j++)
					{
						if (possible_field[id][i][j])
						{
							result = true;
							break;
						}
					}
					if (result)break;
				}
				if (!result)assert(0);
			}
		}
	}


	Battlefield & operator=(const Battlefield & b) {
		memcpy(gamefield, b.gamefield, sizeof(gamefield));
		memcpy(possible_field, b.possible_field, sizeof(possible_field));
		memcpy(self_position, b.self_position, sizeof(self_position));
		memcpy(enemy_position, b.enemy_position, sizeof(enemy_position));
		memcpy(e_stem_action, b.e_stem_action, sizeof(e_stem_action));
		memcpy(tankAlive, b.tankAlive, sizeof(tankAlive));
		memcpy(e_tankAlive, b.e_tankAlive, sizeof(e_tankAlive));
		memcpy(shoot, b.shoot, sizeof(shoot));
		memcpy(e_shoot, b.e_shoot, sizeof(e_shoot));
		memcpy(steps_in_forest, b.steps_in_forest, sizeof(steps_in_forest));
		stem_action = b.stem_action;
		myside = b.myside;
		value = b.value;
		weight = b.weight;
		return *this;
	};
	Battlefield(const Battlefield &b) {
		*this = b;
	}
	Battlefield * init(Json::Value info) {

		memset(gamefield, 0, 81 * sizeof(int));
		memset(possible_field, false, sizeof(possible_field));
		gamefield[0][4] = gamefield[8][4] = base;
		value = 0;
		Json::Value requests = info["requests"], responses = info["responses"];
		//assert(requests.size());
		//load battlefield info
		myside = requests[0u]["mySide"].asInt();
		if (!myside)
		{
			self_position[0] = 2;
			self_position[1] = 0;
			self_position[2] = 6;
			self_position[3] = 0;
			enemy_position[0] = 6;
			enemy_position[1] = 8;
			enemy_position[2] = 2;
			enemy_position[3] = 8;
		}
		else
		{
			self_position[0] = 6;
			self_position[1] = 8;
			self_position[2] = 2;
			self_position[3] = 8;
			enemy_position[0] = 2;
			enemy_position[1] = 0;
			enemy_position[2] = 6;
			enemy_position[3] = 0;
		}
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
		//ready to build a single branch tree for evaluation
		Battlefield * current = this;
		//initialize self-state and enemy-state at the same time
		for (size_t e_round = 1; e_round < requests.size(); e_round++)
		{

			int round = e_round - 1;
			Action tmp_act;
			for (int j = 0; j < 2; j++)
			{
				if (!current->tankAlive[j]) {
					tmp_act.a[j] = -1;
					continue;
				};
				int x = responses[round][j].asInt();
				tmp_act.a[j] = x;
			}
			current->leaf_action.push_back(tmp_act);
			Battlefield tmp_field = *current;
			current->leaf_action[0].stem_field = current;
			current->leaf_action[0].leaf_field.push_back(*current);
			current->leaf_action[0].leaf_field[0].stem_action = &(current->leaf_action[0]);
			current = &(current->leaf_action[0].leaf_field[0]);
			for (int j = 0; j < 2; j++)
			{
				if (tmp_act.a[j] >= 4)current->shoot[j] = true;
				else current->shoot[j] = false;
				if (tmp_act.a[j] == -1 || tmp_act.a[j] >= 4)continue;
				current->self_position[2 * j] += dx[tmp_act.a[j]];
				current->self_position[2 * j + 1] += dy[tmp_act.a[j]];
			}
			if (current->enemy_position[0] == -1) current->e_tankAlive[0] = false;
			if (current->enemy_position[2] == -1) current->e_tankAlive[1] = false;

			for (size_t j = 0; j < 2; j++)
			{
				current->e_stem_action[j] = requests[e_round]["action"][j].asInt();
				for (size_t k = 0; k < 2; k++)
				{
					int pos = requests[e_round]["final_enemy_positions"][2 * j + k].asInt();
					current->enemy_position[2 * j + k] = pos;
				}
				if (current->enemy_position[2 * j] == -1) {
					current->e_tankAlive[j] = false;
					continue;
				}
				else if (current->e_stem_action[j] >= -1 && current->e_stem_action[j] <= 3) {
					current->e_shoot[j] = false;
				}
				else if (isShoot(current->e_stem_action[j])) {
					current->e_shoot[j] = true;
				}
				//enemy go into forest
				if (current->enemy_position[2 * j] == -2) {
					if (isMove(current->e_stem_action[j])) {
						int x = requests[e_round - 1]["final_enemy_positions"][2 * j].asInt() + dx[current->e_stem_action[j]],
							y = requests[e_round - 1]["final_enemy_positions"][2 * j + 1].asInt() + dy[current->e_stem_action[j]];
						current->steps_in_forest[j] = 1;
						current->possible_field[j][y][x] = true;
					}
					else {
						current->steps_in_forest[j]++;
					}
				}
				else {
					if (current->e_stem_action[j] == -2)current->e_shoot[j] = false;
					current->steps_in_forest[j] = 0;
					memset(current->possible_field[j], false, 81 * sizeof(bool));
				}
			}
			for (size_t j = 0; j < requests[e_round]["destroyed_tanks"].size(); j += 2)
			{
				int x = requests[e_round]["destroyed_tanks"][j].asInt(),
					y = requests[e_round]["destroyed_tanks"][j + 1].asInt();
				bool died = false;
				if (current->self_position[0] == x && current->self_position[1] == y) {
					current->tankAlive[0] = false;
					died = true;
				}
				if (current->self_position[2] == x && current->self_position[3] == y) {
					current->tankAlive[1] = false;
					died = true;
				}
				if (died && (current->e_stem_action[0] == -2 || current->e_stem_action[1] == -2)) {
					//shoot by hiding enemy
					set_forest_shooter(current, x, y);
				}
			}

			for (size_t j = 0; j < requests[e_round]["destroyed_blocks"].size(); j += 2)
			{
				int x = requests[e_round]["destroyed_blocks"][j].asInt();
				int y = requests[e_round]["destroyed_blocks"][j + 1].asInt();
				current->gamefield[y][x] = none;
				//shoot by self?
				if (!shoot_by_self(current, x, y)) {
					set_forest_shooter(current, x, y);
				}

			}
			for (size_t j = 0; j < 2; j++)
			{
				if (current->steps_in_forest[j] > 1) {
					if (current->e_stem_action[j] != -2) {
						current->steps_in_forest[j]--;
					}
					else {
						for (int m = 0; m < 9; m++)
						{
							for (int n = 0; n < 9; n++)
							{
								if (current->possible_field[j][m][n] == true && current->gamefield[m][n] == forest) {
									int y[2] = { m - 1 >= 0 ? m - 1 : 0,m + 1 <= 8 ? m + 1 : 8 };
									int x[2] = { n - 1 >= 0 ? n - 1 : 0,n + 1 <= 8 ? n + 1 : 8 };
									current->possible_field[j][y[0]][x[0]] |= (current->gamefield[y[0]][x[0]] == forest);
									current->possible_field[j][y[0]][x[1]] |= (current->gamefield[y[0]][x[1]] == forest);
									current->possible_field[j][y[1]][x[0]] |= (current->gamefield[y[1]][x[0]] == forest);
									current->possible_field[j][y[1]][x[1]] |= (current->gamefield[y[1]][x[1]] == forest);
								}
							}
						}
					}
				}

			}
			current->valid_possiblefield();
		}
		return current;
	}
	Battlefield() {
		stem_action = &root_action;
		stem_action->stem_field = nullptr;
	}
	bool operator<(const Battlefield & b)const { return value < b.value; }

	bool genBranch(int move_limit = 3, int enemy_possible_pos = 3) {
		//stay or move
		map<Battlefield, int> tmp_move[4];
		vector<int> act[4];
		for (size_t i = 0; i < 2; i++)
		{
			if (tankAlive[i] == false) {
				act[i].push_back(-1);
				continue;
			}
			for (int j = -1; j < 4; j++)
			{
				Battlefield current = *this;
				if (current.tmp_self_move(i, j)) {
					current.move_evaluate(i);
					tmp_move[i].insert(pair<Battlefield, int>(current, j));
				}
			}
			for (size_t j = 4; j < 8; j++)
			{
				Battlefield current = *this;
				if (current.self_shoot_valid(i, j)) {
					act[i].push_back(j);
				}
			}
		}
		valid_possiblefield();
		vector<Battlefield> currentlist;
		if (enemy_position[0] == -2 || enemy_position[2] == -2) {
			locate_enemy(currentlist);
		}
		else {
			currentlist.push_back(*this);
		}
		for (size_t index = 0; index < currentlist.size() && index < enemy_possible_pos; index++)
		{
			for (size_t i = 0; i < 2; i++)
			{
				if (e_tankAlive[i] == false) {
					act[i + 2].push_back(-1);
					continue;
				}
				for (int j = -1; j < 4; j++)
				{
					Battlefield current = currentlist[index];
					if (current.tmp_enemy_move(i, j)) {
						current.move_evaluate(i, false);
						tmp_move[i + 2].insert(pair<Battlefield, int>(current, j));
					}
				}
				for (size_t j = 4; j < 8; j++)
				{
					Battlefield current = *this;
					if (current.enemy_shoot_valid(i, j)) {
						act[i + 2].push_back(j);
					}
				}
			}
		}
		map<Battlefield, int>::reverse_iterator it[4];
		for (size_t i = 0; i < 4; i++)
		{
			it[i] = tmp_move[i].rbegin();
			for (int j = 0; j < move_limit && it[i] != tmp_move[i].rend(); j++, it[i]++)
			{
				act[i].push_back(it[i]->second);
			}
		}
		/* before debug
		for (size_t i = 0; i < 4; i++)
		{
			if (act[i].size() == 0)act[i].push_back(-1);
		}
		*/
		for (size_t index = 0; index < currentlist.size() && index < enemy_possible_pos; index++)
		{
			for (auto & i : act[0])
			{
				for (auto & j : act[1])
				{
					Action tmp(i, j);
					if (i == -1 && j == -1)continue;
					bool cut = false;

					for (auto & k : act[2])
					{
						for (auto & l : act[3])
						{
							Battlefield current = currentlist[index];
							bool tank_in_danger[2] = { current.indanger(0),current.indanger(1) };
							int action_node[4] = { i,j,k,l };
							current.quick_play(action_node);
							tank_in_danger[0] &= current.indanger(0);
							tank_in_danger[1] &= current.indanger(1);
							if (current.lose() || tank_in_danger[0] || tank_in_danger[1]) {
								cut = true;
								break;
							}
							current.eval_field();
							tmp.leaf_field.push_back(current);
						}
					}
					if (!cut) {
						tmp.stem_field = this;
						tmp.eval_action();
						leaf_action.push_back(tmp);
					}
				}
			}
		}
		if (leaf_action.size() == 0)
			return false;
		sort(leaf_action.rbegin(), leaf_action.rend());
		for (auto &i : leaf_action)
		{
			for (auto &j : i.leaf_field)
			{
				j.stem_action = &i;
			}
		}
		return true;
	}


private:
	bool tankOK(int x, int y)// can a tank steps in?
	{
		return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (gamefield[y][x] == none || gamefield[y][x] == forest);
	}
	bool lose() {
		if (gamefield[0][4] == none && gamefield[8][4] == none) {
			return false;
		}
		if (gamefield[0][4] == none && myside == 0) return true;
		if (gamefield[8][4] == none && myside == 1) return true;
		if (cost_attack_base(0) != 1 && cost_attack_base(1) != 1) {
			if (!tankAlive[0] && !tankAlive[1])return true;
		}
		return false;

	}
	//when calling this function, locate_enemy must be used first
	bool indanger(int id) {
		if (!tankAlive[id]) return false;
		if (e_shoot[0] == true && e_shoot[1] == true)return false;
		if (cost_attack_base(id) == 1)return false;
		for (size_t i = 0; i < 2; i++)
		{
			if (e_tankAlive[i]&&e_shoot[i] == false && 
				can_shoot(*this, enemy_position[2 * i], enemy_position[2 * i + 1], self_position[2 * id], self_position[2 * id + 1]))
				return true;
		}
		return false;
	}


	bool equal() {
		return gamefield[0][4] == none && gamefield[8][4] == none;
	}
	bool possible_enemy(int x, int y, int dir) {
		for (size_t j = 0; j < 2; j++)
		{
			if (enemy_position[j * 2] == x && enemy_position[j * 2 + 1] == y)return true;
			if (possible_field[j][y][x] == true)return true;
		}
		int neibor[4] = { x + dy[dir],y + dx[dir],x - dy[dir],y - dx[dir] };
		for (size_t i = 0; i < 2; i++)
		{
			if (!tankOK(neibor[i * 2], neibor[i * 2 + 1]))continue;
			for (size_t j = 0; j < 2; j++)
			{
				if (!e_tankAlive[j] == false)continue;
				if (enemy_position[j * 2] == neibor[i * 2] && enemy_position[j * 2 + 1] == neibor[i * 2 + 1])return true;
				if (possible_field[j][neibor[i * 2 + 1]][neibor[i * 2]] == true)return true;
			}
		}
		return false;
	}
	bool possible_self(int x, int y, int dir) {
		for (size_t j = 0; j < 2; j++)
		{
			if (self_position[j * 2] == x && self_position[j * 2 + 1] == y)return true;
		}
		int neibor[4] = { x + dy[dir],y + dx[dir],x - dy[dir],y - dx[dir] };
		for (size_t i = 0; i < 2; i++)
		{
			if (!tankOK(neibor[i * 2], neibor[i * 2 + 1]))continue;
			for (size_t j = 0; j < 2; j++)
			{
				if (!tankAlive[j] == false)continue;
				if (self_position[j * 2] == neibor[i * 2] && self_position[j * 2 + 1] == neibor[i * 2 + 1])return true;
			}
		}
		return false;
	}

	//judge whether (x,y) is shot by our tanks
	static bool shoot_by_self(Battlefield * current, int x, int y) {
		for (size_t i = 0; i < 2; i++)
		{
			if (current->stem_action->a[i] > 3) {
				if (x == current->self_position[2 * i]) {
					int tmp_y = current->self_position[2 * i + 1];
					bool barrier = false;
					if (current->stem_action->a[i] == 4 && current->self_position[2 * i + 1] > y) {
						while (tmp_y >= y) {
							if (current->gamefield[tmp_y][x] == brick || current->gamefield[tmp_y][x] == steel) {
								barrier = true;
								break;
							}
							tmp_y--;
						}
						if (!barrier) return true;
					}
					else if (current->stem_action->a[i] == 6 && current->self_position[2 * i + 1] < y) {
						while (tmp_y <= y) {
							if (current->gamefield[tmp_y][x] == brick || current->gamefield[tmp_y][x] == steel) {
								barrier = true;
								break;
							}
							tmp_y++;
						}
						if (!barrier) return true;
					}
				}
				else if (y == current->self_position[2 * i + 1]) {
					int tmp_x = current->self_position[2 * i];
					bool barrier = false;
					if (current->stem_action->a[i] == 7 && current->self_position[2 * i] > x) {
						while (tmp_x >= x) {
							if (current->gamefield[y][tmp_x] == brick || current->gamefield[y][tmp_x] == steel) {
								barrier = true;
								break;
							}
							tmp_x--;
						}
						if (!barrier) return true;
					}
					else if (current->stem_action->a[i] == 5 && current->self_position[2 * i] < x) {
						while (tmp_x <= x) {
							if (current->gamefield[y][tmp_x] == brick || current->gamefield[y][tmp_x] == steel) {
								barrier = true;
								break;
							}
							tmp_x++;
						}
						if (!barrier) return true;
					}
				}
			}
		}
		return false;
	}
	//when enemy in forest destroyed something, just find the shooter.
	static void set_forest_shooter(Battlefield* current, int x, int y) {
		bool tmp_field[2][9][9];
		memcpy(tmp_field, current->possible_field, sizeof(tmp_field));
		for (int m = y; m >= 0; m--)
		{
			if (current->gamefield[m][x] == none || current->gamefield[m][x] == water)continue;
			if (current->gamefield[m][x] == brick || current->gamefield[m][x] == steel)break;
			for (int j = 0; j < 2; j++)
			{
				if (current->possible_field[j][m][x] == true) {
					current->e_shoot[j] = true;
					current->e_stem_action[j] = 6;
					memset(current->possible_field[j], false, 81 * sizeof(bool));
					for (int k = m; current->gamefield[k][x] == forest && k >= 0; k--)
					{
						current->possible_field[j][k][x] = tmp_field[j][k][x];
					}
				}
			}
		}
		for (size_t i = y; i < 9; i++)
		{
			if (current->gamefield[i][x] == none || current->gamefield[i][x] == water)continue;
			if (current->gamefield[i][x] == brick || current->gamefield[i][x] == steel)break;
			for (int j = 0; j < 2; j++)
			{
				if (current->possible_field[j][i][x] == true) {
					current->e_shoot[j] = true;
					current->e_stem_action[j] = 4;
					memset(current->possible_field[j], false, 81 * sizeof(bool));
					for (size_t k = i; current->gamefield[k][x] == forest && k < 9; k++)
					{
						current->possible_field[j][k][x] = tmp_field[j][k][x];
					}
				}
			}
		}
		for (int i = x; i < 9; i++)
		{
			if (current->gamefield[y][i] == none || current->gamefield[y][i] == water)continue;
			if (current->gamefield[y][i] == brick || current->gamefield[y][i] == steel)break;
			for (int j = 0; j < 2; j++)
			{
				if (current->possible_field[j][y][i] == true) {
					current->e_shoot[j] = true;
					current->e_stem_action[j] = 7;
					memset(current->possible_field[j], false, 81 * sizeof(bool));
					for (int k = i; current->gamefield[y][k] == forest && k < 9; k++)
					{
						current->possible_field[j][y][k] = tmp_field[j][y][k];
					}
				}
			}
		}
		for (int i = x; i >= 0; i--)
		{
			if (current->gamefield[y][i] == none || current->gamefield[y][i] == water)continue;
			if (current->gamefield[y][i] == brick || current->gamefield[y][i] == steel)break;
			for (int j = 0; j < 2; j++)
			{
				if (current->possible_field[j][y][i] == true) {
					current->e_stem_action[j] = 5;
					current->e_shoot[j] = true;
					memset(current->possible_field[j], false, 81 * sizeof(bool));
					for (int k = i; current->gamefield[y][k] == forest && k >= 0; k--)
					{
						current->possible_field[j][y][k] = tmp_field[j][y][k];
					}
				}
			}
		}
		return;
	}

	void quick_play(int * actionlist) {
		e_stem_action[0] = actionlist[2];
		e_stem_action[1] = actionlist[3];
		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id] >= 0 && actionlist[id] < 4) {
				int xx = self_position[id * 2] + dx[actionlist[id]];
				int yy = self_position[id * 2 + 1] + dy[actionlist[id]];
				shoot[id] = false;
				self_position[id * 2] = xx;
				self_position[id * 2 + 1] = yy;
			}
		}
		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id + 2] >= 0 && actionlist[id + 2] < 4) {
				int xx = enemy_position[id * 2] + dx[actionlist[id + 2]];
				int yy = enemy_position[id * 2 + 1] + dy[actionlist[id + 2]];
				e_shoot[id] = false;
				enemy_position[id * 2] = xx;
				enemy_position[id * 2 + 1] = yy;
			}
		}
		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id + 2] >= 4) {
				e_shoot[id] = true;
				int xx = enemy_position[id * 2];
				int yy = enemy_position[id * 2 + 1];
				while (true) {
					xx += dx[actionlist[id + 2] - 4];
					yy += dy[actionlist[id + 2] - 4];
					if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
						break;
					if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
						gamefield[yy][xx] = none;
						break;
					}
					bool tank_shot = false;
					for (size_t i = 0; i < 2; i++)
					{
						if (self_position[i * 2] == xx &&
							self_position[i * 2 + 1] == yy &&
							actionlist[id + 2] != opposite_shoot(actionlist[i])) {
							tankAlive[i] = false;
							tank_shot = true;
						}
					}
					if (tank_shot)break;
				}
			}
		}

		for (size_t id = 0; id < 2; id++)
		{
			if (actionlist[id] >= 4) {
				shoot[id] = true;
				int xx = self_position[id * 2];
				int yy = self_position[id * 2 + 1];
				while (true) {
					xx += dx[actionlist[id] - 4];
					yy += dy[actionlist[id] - 4];
					if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
						break;
					if (gamefield[yy][xx] == base || gamefield[yy][xx] == brick) {
						gamefield[yy][xx] = none;
						break;
					}
					bool tank_shot = false;
					for (size_t i = 0; i < 2; i++)
					{
						if (enemy_position[i * 2] == xx &&
							enemy_position[i * 2 + 1] == yy &&
							actionlist[id] != opposite_shoot(actionlist[i + 2])) {
							e_tankAlive[i] = false;
							tank_shot = true;
						}
					}
					if (self_position[(1 - id) * 2] == xx &&
						self_position[(1 - id) * 2 + 1] == yy) {
						tankAlive[id] = false;
						tank_shot = true;
					}
					if (tank_shot)break;

				}
			}
		}

		for (size_t i = 0; i < 2; i++)
		{
			if (tankAlive[i] == false) {
				self_position[2 * i] = -1; self_position[2 * i + 1] = -1;
			}
		}
		for (size_t i = 0; i < 2; i++)
		{
			if (e_tankAlive[i] == false) {
				enemy_position[2 * i] = -1; enemy_position[2 * i + 1] = -1;
			}
		}
	}
	//temp move function for single tank
	//when call it, the battlefield will be ruined and no longer meaningful
	bool tmp_self_move(int id, int act)
	{
		if (act == -1) {
			stem_action->a[id] = -1;
			shoot[id] = false;
			return true;
		}

		int xx = self_position[id * 2] + dx[act];
		int yy = self_position[id * 2 + 1] + dy[act];
		if (!tankOK(xx, yy))return false;
		for (int i = 0; i < 2; i++)
		{
			if (enemy_position[i * 2] >= 0)//can not step into a tank's block (although tanks can overlap inadventently)
			{
				if ((xx == enemy_position[i * 2]) && (yy == enemy_position[i * 2 + 1]))
					return false;
			}
			if (self_position[i * 2] >= 0)
			{
				if ((xx - self_position[i * 2] == 0) && (yy - self_position[i * 2 + 1] == 0))
					return false;
			}
		}
		shoot[id] = false;
		stem_action->a[id] = act;
		self_position[id * 2] = xx;
		self_position[id * 2 + 1] = yy;
		return true;
	}
	//temp move function for single enemy tank
	//when call it, the battlefield will be ruined and no longer meaningful
	bool tmp_enemy_move(int id, int act) {
		if (e_tankAlive[id] == false && act != -1)return false;

		if (act == -1) {
			e_stem_action[id] = -1;
			e_shoot[id] = false;
			return true;
		}

		int xx = enemy_position[id * 2] + dx[act];
		int yy = enemy_position[id * 2 + 1] + dy[act];
		if (!tankOK(xx, yy))return false;
		for (int i = 0; i < 2; i++)
		{
			if (self_position[i * 2] >= 0)//can not step into a tank's block (although tanks can overlap inadventently)
			{
				if ((xx == self_position[i * 2]) && (yy == self_position[i * 2 + 1]))
					return false;
			}
			if (enemy_position[i * 2] >= 0)
			{
				if ((xx == enemy_position[i * 2]) && (yy == enemy_position[i * 2 + 1]))
					return false;
			}
		}
		e_shoot[id] = false;
		e_stem_action[id] = act;
		enemy_position[id * 2] = xx;
		enemy_position[id * 2 + 1] = yy;
		return true;
	}

	//shooting destroying nothing is not effective when there's no tanks nearby

	bool self_shoot_valid(int id, int act) {
		if (shoot[id])return false;
		int xx = self_position[id * 2];
		int yy = self_position[id * 2 + 1];
		bool shoot_enemy = false;
		while (true) {
			xx += dx[act - 4];
			yy += dy[act - 4];
			if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
				return shoot_enemy;
			shoot_enemy = possible_enemy(xx, yy, act - 4);
			if (gamefield[yy][xx] == base) {
				if (yy == (myside == 0 ? 0 : 8))
					return false;//attack own base
				else return true;
			}
			if (gamefield[yy][xx] == brick) {
				return true;
			}
		}
	}
	bool enemy_shoot_valid(int id, int act) {
		if (e_shoot[id])return false;
		int xx = enemy_position[id * 2];
		int yy = enemy_position[id * 2 + 1];
		bool shoot_self = false;
		while (true) {
			xx += dx[act - 4];
			yy += dy[act - 4];
			if (xx < 0 || xx>8 || yy < 0 || yy>8 || gamefield[yy][xx] == steel)
				return shoot_self;
			shoot_self = possible_self(xx, yy, act - 4);
			if (gamefield[yy][xx] == base) {
				if (yy == (myside == 0 ? 8 : 0))
					return false;
				else return true;//enemy attack his own base
			}
			if (gamefield[yy][xx] == brick) {
				return true;
			}
		}
	}

	bool can_shoot(Battlefield & f, int x, int y, int dst_x, int dst_y) {
		if (x != dst_x || y != dst_y)return false;
		if (x == dst_x) {
			int y_step = y < dst_y ? 1 : -1;
			for (size_t i = y; i != dst_y; i += y_step)
			{
				if (f.gamefield[i][x] == brick || f.gamefield[i][x] == steel)return false;
			}
		}
		else {
			int x_step = x < dst_x ? 1 : -1;
			for (size_t i = x; i != dst_x; i += x_step)
			{
				if (f.gamefield[y][i] == brick || f.gamefield[y][i] == steel)return false;
			}
		}
		return true;
	}
	//locate_enemy using two-method : attack us or attack base
	void locate_enemy(vector<Battlefield> & field) {
		struct Pos {
			int x, y;
		};
		vector<Pos> enemy[2];
		for (size_t id = 0; id < 2; id++)
		{
			if (enemy_position[2 * id] == -2) {
				for (int i = 0; i < 9; i++)
				{
					for (int j = 0; j < 9; j++)
					{
						if (possible_field[id][i][j] == true) {
							enemy[id].push_back(Pos{ j, i });
						}
					}
				}
			}
			else {
				enemy[id].push_back(Pos{ enemy_position[2 * id],enemy_position[2 * id + 1] });
			}
		}
		memset(possible_field, false, sizeof(possible_field));
		for (size_t i = 0; i < enemy[0].size(); i++)
		{
			for (size_t j = 0; j < enemy[1].size(); j++)
			{
				Battlefield current = *this;
				current.enemy_position[0] = enemy[0][i].x;
				current.enemy_position[1] = enemy[0][i].y;
				current.enemy_position[2] = enemy[1][j].x;
				current.enemy_position[3] = enemy[1][j].y;
				current.eval_enemy();
				field.push_back(current);
			}
		}
		if (field.size() == 0)
			return;
		sort(field.rbegin(), field.rend());
	}
	inline int cost_attack_base(int id, bool enemy = false) {
		int x = 4, y =(myside&&enemy) || (!myside && !enemy) ?  8 : 0;
		return cost_attack(id, enemy, x, y);
	}

	int cost_attack(int id, bool enemy,int x,int y) {
		int * tank = enemy ? enemy_position : self_position;
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
		Block now(tank[2 * id], tank[2 * id + 1], 0);
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
						//tmp.gamefield[neibor[i].y][neibor[i].x] = none;
					}
					if (can_shoot(tmp, neibor[i].x, neibor[i].y, x, y)) {
						return neibor[i].cost + 1;
					}
					neibor[i].estimate_cost = abs(neibor[i].x - x) + abs(neibor[i].y - y);
					list.push(neibor[i]);
				}
			}
		}
		return 50;
	}
	//drive our tanks
	void move_evaluate(int id, bool enemy = false) {
		int attack1, attack2;
		double cost[2];
		attack1 = cost_attack(id, enemy, self_position[0], self_position[1]);
		attack2 = cost_attack(id, enemy, self_position[2], self_position[3]);
		cost[0] = attack1 < attack2 ? attack1 : attack2;
		cost[1] = cost_attack_base(id, enemy);
		if (cost[0] > cost[1])cost[0] *= 0.4;
		else cost[1] *= 0.4;
		value = 70 - cost[0] - cost[1];
	}

	void eval_enemy() {
		value = 140;
		int attack1, attack2;
		double cost[4];
		attack1 = cost_attack(0, true, self_position[0], self_position[1]);
		attack2 = cost_attack(0, true, self_position[2], self_position[3]);
		cost[0] = attack1 < attack2 ? attack1 : attack2;
		attack1 = cost_attack(1, true, self_position[0], self_position[1]);
		attack2 = cost_attack(1, true, self_position[2], self_position[3]);
		cost[1] = attack1 < attack2 ? attack1 : attack2;
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

	void eval_field() {
		value = 0;
		for (size_t i = 0; i < 2; i++)
		{
			if(tankAlive[i])value += 50 - cost_attack_base(i)*0.8;
			if(e_tankAlive[i])value -= 50 - cost_attack_base(i, true);
		}
		for (size_t i = 0; i < 2; i++)
		{
			if (!e_tankAlive[i]) {
				weight *= 0.8;
				value += 100;
			}
		}


		if (gamefield[0][4] == none && myside == 1)
			value = 1000;
		if (gamefield[8][4] == none && myside == 0)
			value = 1000;

	}
};
typename Battlefield::Action Battlefield::root_action = Action();


struct Bot {
	Battlefield * field;
	Battlefield root;

	Bot(Json::Value input) :field() {
		field = root.init(input);
	}
	Json::Value bot_main()
	{
		field->genBranch();
		Json::Reader reader;
		Json::Value temp, output;
		output = Json::Value(Json::arrayValue);
		output.append(field->leaf_action[0].a[0]);
		output.append(field->leaf_action[0].a[1]);
		return output;
	}
};

#ifdef _BOTZONE_ONLINE

int main()
{
	srand(time(0));
	Json::Reader reader;
	Json::Value input, output;
	Json::FastWriter writer;
	reader.parse(cin, input);
	Bot bot(input);
	output = bot.bot_main();
	cout << writer.write(output);
	return 0;
}

#endif