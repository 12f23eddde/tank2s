
#include "pch.h"
#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include <queue>
#include <vector>
#include "json/json.h"

#define DEBUG

using namespace std;
const int none = 0, brick = 1, forest = 2, steel = 4, water = 8, tank = 16;
const bool DebugMode = true;
int state[9][9];
int myside;
int rounds;//回合数
int px[4] = { 0,1,0,-1 };//上、右、下、左，对应行动的0 1 2 3(行进) 和 4 5 6 7(射击)
int py[4] = { -1,0,1,0 };
/*全局变量定义结束*/

struct point {
	int x, y;
	double cost;
	friend bool operator <(point a, point b)//不要在意这个奇妙的比较
	{
		return a.cost > b.cost;
	}
};

bool comp_point(point a, point b)
{
	return a.cost < b.cost;
}

struct Distances//搜索以后上下左右四个方向最短时间
{
	double d[4];
};

class Tank {
public:
	int x, y;//位置x y
	bool in_bush = 0;//在树丛中
	int shoot_cnt = 1;//是否可以发射炮弹,以及连续几个回合没有发射
	int dead = 0;//awsl
	bool possible[9][9];//可能处于的位置，仅当敌方存在于树林中时有效
	void cal_pos()//当in_bush为1时，计算可能位置，为0时清空possible数组
	{
		if (in_bush == 0)
		{
			memset(possible, 0, sizeof(possible));
			return;
		}

		for (int i = 0; i < 9; i++)
		{
			for (int j = 0; j < 9; j++)
			{
				if (possible[i][j] == 1 and !!state[i][j] & forest)
				{
					int xx, yy;
					for (int k = 0; k < 4; k++)
					{
						xx = j + px[k];
						yy = i + py[k];
						if (state[yy][xx] & forest)
						{
							possible[yy][xx] = 1;
						}
					}
				}
			}
		}

		return;
	}
};
Tank  self_tank[2];
Tank enemy_tank[2];

bool in_map(int x,int y)//是否在地图内
{
	if (x >= 0 and x <= 8 and y >= 0 and y <= 8) return true;
	else return false;
}

bool ok(int x, int y)// can a tank steps in?
{
	return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (~state[y][x] & steel) && (~state[y][x] & water) && (~state[y][x] & brick) && (~state[y][x] & tank);
}

bool in_range(int self_id, int enemy_id)//自己和敌方两辆坦克是否面对面
{
	if (self_tank[self_id].x != enemy_tank[enemy_id].x and self_tank[self_id].y != enemy_tank[enemy_id].y)//不在同一直线上
		return false;

	if (self_tank[self_id].x == enemy_tank[enemy_id].x)
	{
		int x = self_tank[self_id].x;
		int y1 = self_tank[self_id].y, y2 = enemy_tank[enemy_id].y;
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

	else if (self_tank[self_id].y == enemy_tank[enemy_id].y)
	{
		//大规模代码复用
		int y = self_tank[self_id].y;
		int x1 = self_tank[self_id].x, x2 = enemy_tank[enemy_id].x;
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
}

bool in_range(int enemy_id)//重载后的函数，判断基地是否会被打中，不管这时敌人有没有炮弹都很危险
{
	//再次大规模复用，和上一个几乎一样
	if (4 != enemy_tank[enemy_id].x and myside * 8 != enemy_tank[enemy_id].y)
		return false;

	if (4 == enemy_tank[enemy_id].x)//敌方坦克处于中轴线
	{
		int x = 4;
		int y1 = myside * 8, y2 = enemy_tank[enemy_id].y;
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
			else
				return false;
		}
		return true;
	}

	else if (myside * 8 == enemy_tank[enemy_id].y)//敌方坦克和基地在同一横线上
	{
		int y = myside * 8;
		int x1 = 4, x2 = enemy_tank[enemy_id].x;
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
}

double get_distance(int x, int y, int id)//搜索攻击地方基地的最短距离
{
	if (DebugMode) cout << "x= " << x << " y= " << y << " id= " << id << endl;
	bool explored[9][9], frontier[9][9];
	double min_step = 10000000;
	memset(explored, 0, sizeof(explored));
	memset(frontier, 0, sizeof(frontier));
	if(DebugMode) cout << "self_tank y=" << self_tank[id].y << " x=" << self_tank[id].x << endl;
	explored[self_tank[id].y][self_tank[id].x] = 1;
	priority_queue<point> q;
	point start;
	start.x = x, start.y = y;
	if (!in_map(x, y)) return 10000000;
	if (state[y][x] & steel) return 10000000;
	if (state[y][x] & water and y != 8 * (1 - myside)) return 10000000;
	if (state[y][x] == 0 or state[y][x] & forest) start.cost = 1;
	if (state[y][x] & brick)
	{
		if (self_tank[id].shoot_cnt == 0) start.cost = 3;
		else start.cost = 2;
	}
	q.push(start);
	explored[y][x] = 1, frontier[y][x] = 1;
	while (!q.empty())
	{
		point temp = q.top();
		if (frontier[temp.y][temp.x] == 0)//如果不在探索集中，将其移除
		{
		//	cout << "i wonder if this ever worked" << endl;
			q.pop();
			continue;
		}
		//cout << "out: " << temp.y << ' ' << temp.x <<  ' ' <<temp.cost << endl;
		if (temp.x == 4 and temp.y == 8 * (1 - myside))
		{
			if (temp.cost < min_step) min_step = temp.cost;
		}
		q.pop();
		frontier[temp.y][temp.x] = 0;
		for (int i = 0; i < 4; i++)
		{
			point next;
			next.x = temp.x + px[i];
			next.y = temp.y + py[i];
			if (!in_map(next.x, next.y)) continue;
			if (explored[next.y][next.x] == 1)
			{
				if (frontier[next.y][next.x] == 0)
					continue;
			}
			if (next.y == 8 * (1 - myside) and (i == 1 or i == 3) and (state[next.y][next.x] == 0 or state[next.y][next.x] & water)) next.cost = temp.cost;//只要炮弹能打到即可，不用走到旁边
			if ((state[next.y][next.x] & steel) or (state[next.y][next.x] & water)) continue;
			for (int j = 0; j < 2; j++)
			{
				if (next.y == enemy_tank[j].y and next.x == enemy_tank[j].x) next.cost = temp.cost + 3;
			}
			if (state[next.y][next.x] & brick) next.cost = temp.cost + 2.1;
			else  next.cost = temp.cost + 1;
			q.push(next);
			if(DebugMode) 
			explored[next.y][next.x] = true;
			frontier[next.y][next.x] = true;
			cout << "		in: " << next.y << ' ' << next.x << ' ' << next.cost << endl;
		}
	}
	cout << min_step << endl;
	for (int i = 0; i < 9; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			cout << explored[i][j];
		}
		cout << endl;
	}
	return min_step;
}

Distances route_search(int id)//上下左右四个方向走向敌方基地所需的最短距离，敌方坦克暂且当砖块算 id为坦克编号
{
	Distances temp;
	for (int i = 0; i < 4; i++)
	{
		temp.d[i] = 10000000;
		int xx = self_tank[id].x + px[i];
		int yy = self_tank[id].y + py[i];
		if(DebugMode) cout << "xx= " << xx << " yy= " << yy << endl;
		if (!in_map(xx, yy)) continue;
		temp.d[i] = get_distance(xx, yy, id);
	};
	return temp;
}
	
bool valid(int id, int action)//最粗略的检查
{
#ifdef DEBUG
	cout << endl << id << ' ' << action << endl << endl;
#endif // DEBUG

	if (self_tank[id].dead and action != -1) return false;
	if (self_tank[id].y == (myside) * 8)
	{
	//	if (self_tank[id].x < 4 && action == 5)return false;
	//	if (self_tank[id].x > 4 && action == 7)return false;// 这个比较值得商榷，先注释掉
	}
	if (action == -1 || action >= 4)return true;
	int xx = self_tank[id].x + px[action];
	int yy = self_tank[id].y + py[action];
	if (!ok(xx, yy))return false;
	/*cout << "next location:" << yy << ' ' << xx << endl;
	for (int i = 0; i < 2; i++)
	{
		cout << "enemy" << i << ": " << enemy_tank[i].y << ' ' << enemy_tank[i].x << endl;
		if (enemy_tank[i].x >= 0)//can not step into a tank's block (although tanks can overlap inadventently)
		{
			if ((xx - enemy_tank[i].x == 0) && (yy - enemy_tank[i].y == 0))
			{
				return false;
			}
		}
		if (self_tank[i].x >= 0)
		{
			if ((xx - self_tank[i].x == 0) && (yy - self_tank[i].y == 0))
				return false;
		}
	}*/ //因为有了tank = 16所以这一部分大概不用再留着了
	return true;
}

bool enemy_near(int x, int y, int action)//判断敌人是否在炮弹轨迹旁  即预判有没有打中的可能性 只判断左右两侧 因为直线有in_range
{
	int dir = action % 2; //0 2对应 1 3   1 3 对应 0 2
	int x1, x2, y1, y2;
	x1 = x + px[dir], y1 = y + py[dir], x2 = x + px[dir + 2], y2 = y + py[dir + 2];
	for (int i = 0; i < 2; i++)
	{
		if ((enemy_tank[i].x == x1 and enemy_tank[i].y == y1) or (enemy_tank[i].x == x2 and enemy_tank[i].y == y2))
		{
			return true;
		}
	}
	return false;
}

bool cut(int id, int action)//手动剪枝策略
{
	int state_back[9][9];//备份地图
	memcpy(state_back, state, sizeof(state));
	if (action >= 0 and action < 4)//判断行动后有没有可能被打
	{
		if (!in_map(self_tank[id].x + px[action], self_tank[id].y + py[action])) return false; //new 地图外
		int x = self_tank[id].x, y = self_tank[id].y;
		self_tank[id].x += px[action];
		self_tank[id].y += py[action];
		if ((in_range(id, 0) and enemy_tank[0].shoot_cnt > 0) or (in_range(id, 1) and enemy_tank[1].shoot_cnt > 0))//敌人有炮弹，没有炮弹可以趁机冲过去
		{
			self_tank[id].x = x, self_tank[id].y = y;
			return false;
		}
		if (in_range(0)  or in_range(1))//不管敌人有没有炮弹都很牙白
		{
			self_tank[id].x = x, self_tank[id].y = y;
			return false;
		}
	}
	if (action >= 4)
	{
		if (in_range(id, 0) or in_range(id, 1)) return true;
		//可以打到敌人
		int x = self_tank[id].x + px[action - 4], y = self_tank[id].y + py[action - 4];
		while (in_map(x, y))
		{
			if ( (state[y][x] & steel) or (y == myside * 8 and x == 4) or (y == self_tank[1 - id].y  and x == self_tank[1 - id].x) )//打中钢块或者自己
				return false;
			if (enemy_near(x, y, action)) return true;//预判可以打到敌人
			if (state[y][x] & brick)//如果是砖块
			{
				state[y][x] = 0;
				if ((in_range(id, 0) and enemy_tank[0].shoot_cnt > 0) or (in_range(id, 1) and enemy_tank[1].shoot_cnt > 0))//砖块没了直接白给
				{
					memcpy(state, state_back, sizeof(state));//复原地图
					return false;
				}
				if (in_range(0) or in_range(1))//敌人到老家门口
				{
					memcpy(state, state_back, sizeof(state));//复原地图
					return false;
				}
			}
			x += px[action - 4], y += py[action - 4];
		}//如果结束，说明什么都不会打到
		memcpy(state, state_back, sizeof(state));//复原地图
		return false;
	}
	return true;//以上情况如果都没有发生那大概可以？
}

void move_generator()//用point装一下两个行动 原本的路径cost正好当局面估值（手动滑稽
{
	//原则上坦克两个一组进行搜索，但也会考虑别的
	//即己方0 对方1 和 己方1 对方0 因为分布在中轴线同一侧
	/*目前想好的剪枝规则
	1.行动后会使自己或者基地暴露在敌人火力之下
	2.打了一炮，但是什么也没有发生
	3.掉头往回(这个就通过估值解决吧)
	4.打自己的坦克或者基地，这种情况下可能需要仔细特判
	5.不合法行动（我觉得这个可能不用说）*/
	//以上都是通过自己的行动就可以剪掉的
	//-1作为优先级最低的行动在某些特殊情况下采取
	bool act0[9], act1[9];//tank0 1的行动
	memset(act0, 0, sizeof(act0)), memset(act1, 0, sizeof(act1));//0代表可行，1代表不可行
	for (int i = 0; i < 9; i++)
	{
		if (!valid(0, i - 1)) act0[i] = 1;
		if (!valid(1, i - 1)) act1[i] = 1;
	}
	for (int i = 0; i < 9; i++)
	{
		if (act0[i] and !cut(0, i - 1)) act0[i] = 1;
		if (act1[i] and !cut(1, i - 1)) act1[i] = 1;
	}
#ifdef DEBUG
	cout << "act0: ";
	for (int i = 0; i < 9; i++)
	{
		cout << i - 1 << ":" << act0[i] << ' ';
	}
	cout << endl;
	cout << "act1: ";
	for (int i = 0; i < 9; i++)
	{
		cout << i - 1 << ":" << act1[i] << ' ';
	}
	cout << endl;
#endif // DEBUG

	
}

void init()
{
	//memset(enemy_position, -1, sizeof(enemy_position));
	enemy_tank[0].x = enemy_tank[0].y = enemy_tank[1].x = enemy_tank[1].y = -1;
	self_tank[0].dead = self_tank[1].dead = enemy_tank[0].dead = enemy_tank[1].dead = 0;
 	state[0][4] = state[8][4] = brick;
}

int main()
{
	Json::Reader reader;
	Json::Value input, temp, all, output, debug;
#ifdef _BOTZONE_ONLINE
	reader.parse(cin, all);
#else
	string s = string("{\"requests\":[{\"brickfield\":[4223528,37967378,10645520],\"forestfield\":[129206656,18612716,822863],\"mySide\":1,\"steelfield\":[0,2140160,0],\"waterfield\":[1024,8388608,65568]}],\"responses\":[]}");

	reader.parse(s, all);
#endif

	init();
	int seed = 0;
	input = all["requests"];
	rounds = input.size();
	for (int i = 0; i < input.size(); i++)
	{
		if (i == 0)// read in the map information
		{
			myside = input[0u]["mySide"].asInt();
			if (!myside)
			{
				self_tank[0].x = 2;
				self_tank[0].y = 0;
				self_tank[1].x = 6;
				self_tank[1].y = 0;
			}
			else
			{
				self_tank[0].x = 6;
				self_tank[0].y = 8;
				self_tank[1].x = 2;
				self_tank[1].y = 8;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = input[0u]["brickfield"][j].asInt();
				seed ^= x;
				for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * brick;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = input[0u]["forestfield"][j].asInt();
				for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * forest;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = input[0u]["steelfield"][j].asInt();
				for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * steel;
			}
			for (unsigned j = 0; j < 3; j++)
			{
				int x = input[0u]["waterfield"][j].asInt();
				for (int k = 0; k < 27; k++)state[j * 3 + k / 9][k % 9] |= (!!((1 << k)&x)) * water;
			}
		}
		else// update enemy_position
		{
			for (int j = 0; j < 2; j++)
			{
				int x_t, y_t;//t代表temp
				x_t = input[i]["final_enemy_positions"][2 * j].asInt();
				y_t = input[i]["final_enemy_positions"][2 * j + 1].asInt();
				if (x_t == -2 and y_t == -2)
				{
					if (enemy_tank[j].in_bush == 0)//上一回合尚未进入树林，那么这一回合在树林中位置是可以确定的
					{
						int act = input[i]["action"][j].asInt();
						enemy_tank[j].x += px[act];
						enemy_tank[j].y += py[act];
						enemy_tank[j].cal_pos();
						enemy_tank[j].in_bush = 1;
						enemy_tank[j].possible[enemy_tank[j].y][enemy_tank[j].x] = 1;
					}
					else //上一回合不在树林内，根据储存的possible数组计算出这一回合可能在的所有位置
					{
						enemy_tank[j].cal_pos();
					}
				}
				else
				{
					enemy_tank[j].in_bush = 0;
					enemy_tank[j].x = x_t;
					enemy_tank[j].y = y_t;
				}
				seed += enemy_tank[j].x, seed ^= enemy_tank[j].x;
				seed += enemy_tank[j].y, seed ^= enemy_tank[j].y;
				cout << input[i]["final_enemy_positions"][2 * j + 1].asInt() << ' ' << input[i]["final_enemy_positions"][2 * j].asInt() << endl;
				//enemy_position[j] = input[i]["final_enemy_positions"][j].asInt(), seed += enemy_position[j], seed ^= enemy_position[j];
			}
				
			for (int j = 0; j < input[i]["destroyed_blocks"].size(); j += 2)//可以根据这个推理敌方位置,暂时还没做
			{
				int x = input[i]["destroyed_blocks"][j].asInt();
				int y = input[i]["destroyed_blocks"][j + 1].asInt();
				seed ^= x ^ y;
				state[y][x] = 0;
			}
		}
	}
	
	for (int i = 0; i < 2; i++)//把tank所在位置更新为16
	{
		state[self_tank[i].y][self_tank[i].x] = 16;
		if (enemy_tank[i].x >= 0)//没死且不在草丛里
		{
			state[enemy_tank[i].y][enemy_tank[i].x] = 16;
		}
	} 

	cout << "enemy " << enemy_tank[0].y << ' ' << enemy_tank[0].x << endl << enemy_tank[1].y << ' ' << enemy_tank[1].x << endl;

	input = all["responses"];
	for (int i = 0; i < input.size(); i++)//update self state
	{
		for (int j = 0; j < 2; j++)
		{
			int x = input[i][j].asInt();
			seed ^= x * 2354453456;
			if (x >= 4)self_tank[j].shoot_cnt = 0;
			else self_tank[j].shoot_cnt++;
			if (x == -1 || x >= 4)continue;
			self_tank[j].x += px[x];
			self_tank[j].y += py[x];
			cout << "tank" << j << ": " << self_tank[j].y << ' ' << self_tank[j].x << endl;
		}
	}

	input = all["requests"];
	for (int i = 1; i < input.size(); i++)
	{
		for (int j = 0; j < 2; j++)
		{
			for (int k = 0; k < input[i]["destroyed_tanks"].size(); k += 2)
			{
				int x = input[i]["destroyed_tanks"][k].asInt();
				int y = input[i]["destroyed_tanks"][k + 1].asInt();
				if (self_tank[j].x == x and self_tank[j].y == y)
				{
					self_tank[j].dead = 1;
				}
				if (enemy_tank[j].x == x and enemy_tank[j].y == y)
				{
					enemy_tank[j].dead = 1;
				}
			}
		}
	}
	

	output = Json::Value(Json::arrayValue);
	debug = Json::Value(Json::arrayValue);
	//srand(self_tank[0].x ^ self_tank[1].x ^ enemy_tank[0].y ^ enemy_tank[1].y ^ seed ^ 565646411);
	move_generator();
	int action;
	for (int i = 0; i < 2; i++)
	{
		debug.append("tank"), debug.append(i);
		Distances dis;
		dis = route_search(i);
		int direction = -1;
		double min = 10000;
		for (int j = 0; j < 4; j++)
		{
		//	cout << dis.d[j] << ' ' << min << endl;
			debug.append(j), debug.append(dis.d[j]), debug.append(min);
			if (dis.d[j] < min)
			{
				min = dis.d[j];
				direction = j;
			}
		}
		//cout << "d: " << direction << endl;
		int xx = self_tank[i].x + px[direction];
		int yy = self_tank[i].y + py[direction];
		if (state[yy][xx] & brick)
		{
			if (self_tank[i].shoot_cnt > 0) action = direction + 4;
			else action = -1;
		}
		else action = direction;
		if (!valid(i, action)) action = -1;
		output.append(action);
	}
	//output.append(debug);
	Json::FastWriter writer;
	cout << writer.write(output);
	cout << writer.write(debug);
//	cout << "wrong?" << endl;
	return 0;
}


// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
