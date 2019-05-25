// Tank2S ���ز��г���
// ����Bot�޸Ķ���
// ����������ƽ̨�������ǳ�����
// �Ҳ���Ĭ��Ϊ��ʱ����


#define _CRT_SECURE_NO_WARNINGS 1
#define _BOTZONE_ONLINE

#include<vector>
#include<queue>
#include<map>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include "jsoncpp/json.h"

namespace Judge {
	using namespace std;
	const int dx[4] = { 0,1,0,-1 };
	const int dy[4] = { -1,0,1,0 };
	const int none = 0, brick = 1, steel = 2, water = 4, forest = 8, base = 16;
	const int blue = 0, red = 1, nowinner = 2, game_continue = 3;
	inline bool CoordValid(int x, int y)
	{
		return x >= 0 && x < 9&& y >= 0 && y < 9;
	}

	inline bool is_move(int action) { return action >= 0 && action <= 3; }
	inline bool is_shoot(int action) { return action >= 4; }
	inline int opposite_shoot(int act) { return act > 5 ? act - 2 : act + 2; }

	// ȫ��
	int self, enemy;
	bool vague_death = false;

	struct Pos {
		int x, y;
	};

	struct Action {
		int a[2];
	};


	struct Tank {
		int x, y;
		int action = -1;
		int steps_in_forest = 0;
		inline void setpos(int _x, int _y) {
			if (x == -2)steps_in_forest++;
			else steps_in_forest = 0;
			x = _x, y = _y;
		}
		inline void setact(int act) { action = act; }
		inline void die() { x = -1; setact(-1); }
		inline bool alive() const { return x != -1; }
		inline bool can_shoot()const {
			return action < 4;
		}
		inline bool in_forest() { return x != -2; }
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
		vector<Pos> destroyed_tanks;
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

		void debug_print() {

		}

		// quick_play
		// �����Ϸ���
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
			vector<Pos> distroylist;
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
							distroylist.push_back(Pos{ xx, yy });
							break;
						}
						for (size_t i = 0; i < 2; i++)
						{
							if (tank[self][i].at(xx, yy)) {
								if (tank[self][1 - i] == tank[self][i]) {
									// �ص�̹�˱��������ȫը
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
							distroylist.push_back(Pos{ xx, yy });
							break;
						}
						for (size_t i = 0; i < 2; i++)
						{
							if (tank[enemy][i].at(xx, yy)) {
								if (tank[enemy][1 - i] == tank[enemy][i]) {
									// �ص�̹�˱��������ȫը
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
			// ����������
			for (auto & i : distroylist)gamefield[i.y][i.x] = none;
			for (size_t side = 0; side < 2; side++)
			{
				for (size_t id = 0; id < 2; id++)
				{
					if (tank_die[side][id])tank[side][id].die();
				}
			}

		}

		int play(int * actionlist) {
			bool lose[2] = { false,false };
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(blue, id, actionlist[id]))
				{
					lose[blue] = false;
					cout << "DEBUG��Ϣ��������Ϊ���Ϸ�\n";
					cout << "Tank id: " << id << "  Action: " << actionlist[id] << endl;
				}
			}
			for (size_t id = 0; id < 2; id++)
			{
				if (!move_valid(red, id, actionlist[id + 2]))
				{
					lose[red] = false;
					cout << "DEBUG��Ϣ���췽��Ϊ���Ϸ�\n";
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

		// ��֤�ƶ��Ƿ�Ϸ�
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
		// Judge ���ṹ
		// ��Judge��Ҫ�ṩ����ѵ�� û�и��̹���
		// ��֧��botzone
		// ������Ҫ���ɸ�Ϊvector<Battlefield>�洢ÿ�غ���Ϣ
		Battlefield field;
		Json::Value output;

		// �������� ��Щ������initfield�б�һһ����
		// ÿһλ�������ã��������ʼ��
		int fieldBinary[3];
		int waterBinary[3];					
		int steelBinary[3];
		int	forestBinary[3];
		// ��׼����������
		// ����������������˺ܶ�ȫ�ֱ���
		// ��������rand()���� ���ܳ�Ϊ����ƿ��
		
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
		// ����BFS��֤��ͨ��EnsureConnected
		// ͬ�����ܳ�Ϊ����ƿ��	
			
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
		
		Judge() {
			InitializeField();
			
		}
	};
}



int main()
{

	return 0;
}
