
#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cmath>
#include <cstring>
#include <queue>
#include <vector>
#include "jsoncpp/json.h"

using namespace std;
const int none = 0, brick = 1, forest = 2, steel = 4, water = 8, tank = 16;
const bool DebugMode = 0;
int state[9][9];
int myside;
int rounds;//�غ���
int px[4] = { 0,1,0,-1 };//�ϡ��ҡ��¡��󣬶�Ӧ�ж���0 1 2 3(�н�) �� 4 5 6 7(���)
int py[4] = { -1,0,1,0 };
/*ȫ�ֱ����������*/
#define not_DEBUG
struct point {
	int x, y;
	double cost;
	friend bool operator <(point a, point b)//��Ҫ�����������ıȽ�
	{
		return a.cost > b.cost;
	}
};

bool comp_point(point a, point b)
{
	return a.cost < b.cost;
}

struct Distances//�����Ժ����������ĸ��������ʱ��
{
	double d[4];
};

class Tank {
public:
	int x, y;//λ��x y
	bool in_bush = 0;//��������
	int shoot_cnt = 1;//�Ƿ���Է����ڵ�,�Լ����������غ�û�з���
	int dead = 0;//awsl
	bool possible[9][9];//���ܴ��ڵ�λ�ã������з�������������ʱ��Ч
	void cal_pos()//��in_bushΪ1ʱ���������λ�ã�Ϊ0ʱ���possible����
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

bool in_map(int x,int y)//�Ƿ��ڵ�ͼ��
{
	if (x >= 0 and x <= 8 and y >= 0 and y <= 8) return true;
	else return false;
}

bool ok(int x, int y)// can a tank steps in?
{
	return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (~state[y][x] & steel) && (~state[y][x] & water) && (~state[y][x] & brick) && (~state[y][x] & tank);
}

bool in_range(int self_id, int enemy_id)//�Լ��͵з�����̹���Ƿ������
{
	if (self_tank[self_id].x != enemy_tank[enemy_id].x and self_tank[self_id].y != enemy_tank[enemy_id].y)//����ͬһֱ����
		return false;

	if (self_tank[self_id].x == enemy_tank[enemy_id].x)
	{
		int x = self_tank[self_id].x;
		int y1 = self_tank[self_id].y, y2 = enemy_tank[enemy_id].y;
		if (y1 == y2) return false;//��ͬһ��������Ӧ�ò������뽭��
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
			else//�м����ڵ��������
				return false;
		}
		return true;
	}

	else if (self_tank[self_id].y == enemy_tank[enemy_id].y)
	{
		//���ģ���븴��
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

bool in_range(int enemy_id)//���غ�ĺ������жϻ����Ƿ�ᱻ���У�������ʱ������û���ڵ�����Σ��
{
	//�ٴδ��ģ���ã�����һ������һ��
	if (4 != enemy_tank[enemy_id].x and myside * 8 != enemy_tank[enemy_id].y)
		return false;

	if (4 == enemy_tank[enemy_id].x)//�з�̹�˴���������
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

	else if (myside * 8 == enemy_tank[enemy_id].y)//�з�̹�˺ͻ�����ͬһ������
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

double get_distance(int x, int y, int id)//���������ط����ص���̾���
{
	//�����¹��ܣ�����з����룬�з�̹����ID 2 3����
	//��ȷ����û��bug
	Tank *tank;
	if (id < 2)
		tank = &self_tank[id];
	else
		tank = &enemy_tank[id - 2];
	
	if (DebugMode) cout << "x= " << x << " y= " << y << " id= " << id << endl;
	bool explored[9][9], frontier[9][9];
	double min_step = 10000000;
	memset(explored, 0, sizeof(explored));
	memset(frontier, 0, sizeof(frontier));
	if(DebugMode) cout << "self_tank y=" << tank->y << " x=" << tank->x << endl;
	explored[tank->y][tank->x] = 1;
	priority_queue<point> q;
	point start;
	start.x = x, start.y = y; 
	if (!in_map(x, y)) return 10000000;
	if (state[y][x] & steel) return 10000000;
	if (state[y][x] & water and y != 8 * (1 - myside)) return 10000000;
	if (state[y][x] == 0 or state[y][x] & forest) start.cost = 1;
	if (state[y][x] & brick)
	{
		if (tank->shoot_cnt == 0) start.cost = 3;
		else start.cost = 2;
	}
	q.push(start);
	explored[y][x] = 1, frontier[y][x] = 1;
	while (!q.empty())
	{   
		point temp = q.top();
		if (frontier[temp.y][temp.x] == 0)//�������̽�����У������Ƴ�
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
			if (next.y == 8 * (1 - myside) and (i == 1 or i == 3) and (state[next.y][next.x] == 0 or state[next.y][next.x] & water)) next.cost = temp.cost;//ֻҪ�ڵ��ܴ򵽼��ɣ������ߵ��Ա�
			if ((state[next.y][next.x] & steel) or (state[next.y][next.x] & water)) continue;
			for (int j = 0; j < 2; j++)
			{
				if (id < 2)
				{
					if (next.y == enemy_tank[j].y and next.x == enemy_tank[j].x) next.cost = temp.cost + 3;
				}
				else
				{
					if (next.y == self_tank[j].y and next.x == self_tank[j].x) next.cost = temp.cost + 3;
				}
			}
			if (state[next.y][next.x] & brick) next.cost = temp.cost + 2.1;
			else  next.cost = temp.cost + 1;
			q.push(next);
		//	if(DebugMode) 
			explored[next.y][next.x] = true;
			frontier[next.y][next.x] = true;
		//	cout << "		in: " << next.y << ' ' << next.x << ' ' << next.cost << endl;
		}
	}
/*	cout << min_step << endl;
	for (int i = 0; i < 9; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			cout << explored[i][j];
		}
		cout << endl;
	}*/
	return min_step;
}

Distances route_search(int id)//���������ĸ���������з������������̾��룬�з�̹�����ҵ�ש���� idΪ̹�˱��
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

double evaluate()
{
	//�����˹�ֵ����
	double eval_self = 0, eval_enemy = 0;
	if (!self_tank[0].dead and !self_tank[1].dead)
	{
		double a, b;
		a = get_distance(self_tank[0].x, self_tank[0].y, 0);
		b = get_distance(self_tank[1].x, self_tank[1].y, 1);
		if (a < b)
			eval_self = 2 / a + 1 / b;
		else
			eval_self = 1 / b + 2 / b;//��������ֵΪ��ý���̹�˵ľ���ĵ��������� �������Զ��̹��
	}
	else
	{
		if (self_tank[0].dead)
		{
			eval_self = 1/get_distance(self_tank[1].x, self_tank[1].y, 1);
		}
		if (self_tank[1].dead)
		{
			eval_self = 1/get_distance(self_tank[0].x, self_tank[0].y, 0);
		}
		else
			eval_self = 0;
	}

	if (!enemy_tank[0].dead and !enemy_tank[1].dead)
	{
		double a, b;
		a = get_distance(enemy_tank[0].x, enemy_tank[0].y, 2);
		b = get_distance(enemy_tank[1].x, enemy_tank[1].y, 3);
		if (a < b)
			eval_enemy = 2 / a + 1 / b;
		else
			eval_enemy = 1 / b + 2 / b;//��������ֵΪ��ý���̹�˵ľ���������Զ�ĵ�����
	}
	else
	{
		if (enemy_tank[0].dead)
		{
			eval_enemy = 1/get_distance(enemy_tank[1].x, enemy_tank[1].y, 1);
		}
		if (enemy_tank[1].dead)
		{
			eval_enemy = 1/get_distance(enemy_tank[0].x, enemy_tank[0].y, 0);
		}
		else
			eval_enemy = 0;
	}
	//cout << eval_self << ' ' << eval_enemy << endl;
	return eval_self - eval_enemy;
}

bool valid(int id, int action)//����Եļ��
{
#ifdef DEBUG
	cout << "action: " << id << ' ' << action << endl << endl;
#endif // DEBUG

	if (self_tank[id].dead and action != -1) return false;
	if (self_tank[id].y == (myside) * 8)
	{
	//	if (self_tank[id].x < 4 && action == 5)return false;
	//	if (self_tank[id].x > 4 && action == 7)return false;// ����Ƚ�ֵ����ȶ����ע�͵�
	}
	if (action == -1)return true;
	if (action >= 4)
	{
		if (self_tank[id].shoot_cnt > 0) return true;
		else return false;
	}
	int xx = self_tank[id].x + px[action];
	int yy = self_tank[id].y + py[action];
	if (!ok(xx, yy))
	{
		//cout << yy << ' ' << xx << ' ' << state[yy][xx] << endl;
		return false;
	}
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
	}*/ //��Ϊ����tank = 16������һ���ִ�Ų�����������
	return true;
}

bool enemy_near(int x, int y, int action)//�жϵ����Ƿ����ڵ��켣��  ��Ԥ����û�д��еĿ����� ֻ�ж��������� ��Ϊֱ����in_range
{
	int dir = action % 2; //0 2��Ӧ 1 3   1 3 ��Ӧ 0 2
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

bool cut(int id, int action)//�ֶ���֦���� true���� false����
{
	int state_back[9][9];//���ݵ�ͼ
	memcpy(state_back, state, sizeof(state));
	if (action >= 0 and action < 4)//�ж��ж�����û�п��ܱ���
	{
		if (!in_map(self_tank[id].x + px[action], self_tank[id].y + py[action])) return false; //new ��ͼ��
		int x = self_tank[id].x, y = self_tank[id].y;
		self_tank[id].x += px[action];
		self_tank[id].y += py[action];
		if ((in_range(id, 0) and enemy_tank[0].shoot_cnt > 0) or (in_range(id, 1) and enemy_tank[1].shoot_cnt > 0))//�������ڵ���û���ڵ����Գû����ȥ
		{
			self_tank[id].x = x, self_tank[id].y = y;
			return true;
		}
		if (in_range(0)  or in_range(1))//���ܵ�����û���ڵ���������
		{
			self_tank[id].x = x, self_tank[id].y = y;
			return true;
		}
		self_tank[id].x = x, self_tank[id].y = y;
	}
	if (action >= 4)
	{
		
		if (in_range(id, 0) or in_range(id, 1))//�Ķ��������˷����ڵ��ķ�������Ѿ��������ֻ�ܳ����˴�
		{
			//cout << "IN_RANGE!!!!!!!" << endl;
			if (in_range(id, 0))
			{
				if ((enemy_tank[0].x - self_tank[id].x) == abs(enemy_tank[0].x - self_tank[id].x) * px[action - 4]
					and (enemy_tank[0].y - self_tank[id].y) == abs(enemy_tank[0].y - self_tank[id].y) * py[action - 4])
					return false;
				else return true;
			}
			else
			{
				if ((enemy_tank[1].x - self_tank[id].x) == abs(enemy_tank[1].x - self_tank[id].x) * px[action - 4]
					and (enemy_tank[1].y - self_tank[id].y) == abs(enemy_tank[1].y - self_tank[id].y) * py[action - 4])
					return false;
				else return true;
			}
		}
		//���Դ򵽵���
		int x = self_tank[id].x + px[action - 4], y = self_tank[id].y + py[action - 4];
		while (in_map(x, y))
		{
#ifdef DEBUG
			cout << id << ' ' << action << endl;
			cout << "y:" << y << " x:" << x << ' ';
			cout << "state:"<< state[y][x] << endl;
#endif // DEBUG

			
			if ( (state[y][x] & steel) or (y == myside * 8 and x == 4) or (y == self_tank[1 - id].y  and x == self_tank[1 - id].x) )//���иֿ�����Լ�
				return true;
			if (enemy_near(x, y, action)) return false;//Ԥ�п��Դ򵽵���
			if (state[y][x] & brick)//�����ש��
			{
				state[y][x] = 0;
				if ((in_range(id, 0) and enemy_tank[0].shoot_cnt > 0) or (in_range(id, 1) and enemy_tank[1].shoot_cnt > 0))//ש��û��ֱ�Ӱ׸�
				{
					memcpy(state, state_back, sizeof(state));//��ԭ��ͼ
					return true;
				}
				if (in_range(0) or in_range(1))//���˵��ϼ��ſ�
				{
					memcpy(state, state_back, sizeof(state));//��ԭ��ͼ
					return true;
				}
				else
					return false;//ֻ�Ǵ���ש��
			}
			x += px[action - 4], y += py[action - 4];
		}//���������˵��ʲô�������
		memcpy(state, state_back, sizeof(state));//��ԭ��ͼ
		return true;
	}
	return false;//������������û�з����Ǵ�ſ��ԣ�
}

point move_generator()//��pointװһ�������ж� ԭ����·��cost���õ������ֵ���ֶ�����
{
	//ԭ����̹������һ�������������Ҳ�ῼ�Ǳ��
	//������0 �Է�1 �� ����1 �Է�0 ��Ϊ�ֲ���������ͬһ��
	/*Ŀǰ��õļ�֦����
	1.�ж����ʹ�Լ����߻��ر�¶�ڵ��˻���֮��
	2.����һ�ڣ�����ʲôҲû�з���
	3.��ͷ����(�����ͨ����ֵ�����)
	4.���Լ���̹�˻��߻��أ���������¿�����Ҫ��ϸ����
	5.���Ϸ��ж����Ҿ���������ܲ���˵��*/
	//���϶���ͨ���Լ����ж��Ϳ��Լ�����
	//-1��Ϊ���ȼ���͵��ж���ĳЩ��������²�ȡ
	//������ܳ������������·һ�������
	//��������¾;�����һ��ש�����Ϊ��һ����ȡʱ��
	//��-1��Ϊ������ζ�������
	bool act0[9], act1[9];//tank0 1���ж�
	memset(act0, 0, sizeof(act0)), memset(act1, 0, sizeof(act1));//0������У�1��������
	point actions;
	for (int i = 1; i < 9; i++)
	{
		if (!valid(0, i - 1)) act0[i] = 1;
		if (!valid(1, i - 1)) act1[i] = 1;
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
	for (int i = 1; i < 9; i++)
	{
		if (!act0[i] and cut(0, i - 1)) act0[i] = 1;
		if (!act1[i] and cut(1, i - 1)) act1[i] = 1;
	}
	
	int act[2];
	act[0] = act[1] = -1;
	for (int i = 0; i < 2; i++)
	{
		Distances dis;
		dis = route_search(i);
		int order[4] = {0,1,2,3};
		double temp;
		for (int j = 0; j < 4; j++)
		{
			for (int k = j; k < 4; k++)
			{
				if (dis.d[j] > dis.d[k])
				{
					temp = dis.d[j];
					dis.d[j] = dis.d[k];
					dis.d[k] = temp;
					temp = order[j];
					order[j] = order[k];
					order[k] = temp;
				}
			}
		}
#ifdef DEBUG
		cout << "order" << i << ": ";
		for (int j = 0; j < 4; j++)
		{
			cout << order[j] << ' ';
		}
		cout << endl;
#endif // DEBUG

		//cout << "d: " << direction << endl;
		int xx;
		int yy;
		for (int j = 0; j < 4; j++)
		{
			if (i == 0)
			{
				if (!act0[order[j] + 1])
				{
					act[0] = order[j];
					break;
				}
				else if (!act0[order[j] + 5])
				{
					act[0] = order[j] + 4;
					break;
				}
			}
			else
			{
				if (!act1[order[j] + 1])
				{
					act[1] = order[j];
					break;
				}
				else if (!act1[order[j] + 5])
				{
					act[1] = order[j] + 4;
					break;
				}
			}
		}
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
	actions.x = act[0], actions.y = act[1];
	actions.cost = 0;
	return actions;
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
	string s = string("{\"requests\":[{\"brickfield\":[21167146,4937360,44169748],\"forestfield\":[12320768,127926319,488],\"mySide\":1,\"steelfield\":[512,43008,131072],\"waterfield\":[33554432,1048640,2]},{\"action\":[6,6],\"destroyed_blocks\":[2,1,2,7,6,1,6,7],\"destroyed_tanks\":[],\"final_enemy_positions\":[2,0,6,0]},{\"action\":[2,2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[2,1,6,1]},{\"action\":[2,6],\"destroyed_blocks\":[2,6,6,2],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,6,1]},{\"action\":[-2,2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,6,2]},{\"action\":[-2,3],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,-2,-2]},{\"action\":[-2,-2],\"destroyed_blocks\":[3,4,4,5,5,4],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,-2,-2]},{\"action\":[-2,-2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,4,-2,-2]},{\"action\":[4,-2],\"destroyed_blocks\":[1,5,3,1,5,7],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,4,-2,-2]},{\"action\":[-1,-2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,4,5,4]},{\"action\":[6,6],\"destroyed_blocks\":[3,0,3,7,4,6,5,8],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,4,5,4]},{\"action\":[2,2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,-2,-2]},{\"action\":[-2,-2],\"destroyed_blocks\":[4,7],\"destroyed_tanks\":[],\"final_enemy_positions\":[-2,-2,-2,-2]},{\"action\":[-2,-2],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroye") + string("d_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[3,7,5,7]},{\"action\":[-1,7],\"destroyed_blocks\":[],\"destroyed_tanks\":[3,7],\"final_enemy_positions\":[-1,-1,5,7]},{\"action\":[-1,3],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,4,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,4,7]},{\"action\":[-1,3],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,3,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[3,3],\"final_enemy_positions\":[-1,-1,3,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,3,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,3,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,3,7]},{\"action\":[-1,1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,4,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,4,7]},{\"action\":[-1,-1],\"destroyed_blocks\":[],\"destroyed_tanks\":[],\"final_enemy_positions\":[-1,-1,4,7]}],\"responses\":[[4,4,[\"tank\",0,0,14.4,10000,1,17.5,14.4,2,10000000,14.4,3,15.5,14.4,\"tank\",1,0,15.5,10000,1,15.5,15.5,2,10000000,15.5,3,17.5,15.5]],[0,0,[\"tank\",0,0,14.4,10000,1,19.5,14.4,2,10000000,14.4,3,17.5,14.4,\"tank\",1,0,15.5,10000,1,17.5,15.5,2,10000000,15.5,3,17.5,15.5]],[0,4,[\"tank\",0,0,13.4,10000,1,15.4,13.4,2,16.5,13.4,3,13.4,13.4,\"tank\",1,0,13.4,10000,1,13.4,13.4,2,16.5,13.4,3,16.3,13.4]],[0,0,[\"tank\",0,0,12.4,10000,1,14.4,12.4,2,15.5,12.4,3,12.4,12.4,\"tank\",1,0,13.4,10000,1,15.4,13.4,2,19.6,13.4,3,17.4,13.4]],[3,1,[\"tank\",0,0,10000000,10000,1,15.4,10000,2,13.4,15.4,3,11.4,13.4,\"tank\",1,0,10000000,10000,1,12.4,10000,2,15.5,12.4,3,10000000,12.4]],[7,0,[\"tank\",0,0,10.3,10000,1,15.3,10.3,2,13.3,10.3,3,11.3,10.3,\"tank\",1,0,10.3,10000,1,12.") + string("3,10.3,2,15.4,10.3,3,17.4,10.3]],[3,0,[\"tank\",0,0,9.299999999999999,10000,1,15.3,9.299999999999999,2,13.3,9.299999999999999,3,10.2,9.299999999999999,\"tank\",1,0,8.2,10000,1,11.3,8.2,2,14.4,8.2,3,10000000,8.2]],[7,4,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,12.3,10.3,3,10.3,10.3,\"tank\",1,0,8.299999999999999,10000,1,10000000,8.299999999999999,2,12.3,8.299999999999999,3,10000000,8.299999999999999]],[-1,-1,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,13.2,10.3,3,9.2,10.3,\"tank\",1,0,7.199999999999999,10000,1,10000000,7.199999999999999,2,12.3,7.199999999999999,3,10000000,7.199999999999999]],[6,4,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,11.2,10.3,3,9.2,10.3,\"tank\",1,0,7.199999999999999,10000,1,10000000,7.199999999999999,2,12.3,7.199999999999999,3,10000000,7.199999999999999]],[2,-1,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[6,4,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[2,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[0,4,[\"tank\",0,0,10.1,10000,1,12.1,10.1,2,14.2,10.1,3,11.1,10.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[0,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[2,4,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[0,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[2,4,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[0,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[2,4,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,6") + string(".1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[0,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[3,0,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,6.1,10000,1,10000000,6.1,2,12.3,6.1,3,10000000,6.1]],[-1,-1,[\"tank\",0,0,7.1,10000,1,11.3,7.1,2,13.3,7.1,3,10000000,7.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[-1,-1,[\"tank\",0,0,7.1,10000,1,11.3,7.1,2,13.3,7.1,3,10000000,7.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[4,4,[\"tank\",0,0,7.1,10000,1,11.3,7.1,2,13.3,7.1,3,10000000,7.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[1,-1,[\"tank\",0,0,7.1,10000,1,11.3,7.1,2,13.3,7.1,3,10000000,7.1,\"tank\",1,0,5.1,10000,1,9.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[2,-1,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[0,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[2,-1,[\"tank\",0,0,10000000,10000,1,10.3,10000,2,10.1,10.3,3,8.1,10.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[4,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]],[-1,-1,[\"tank\",0,0,9.1,10000,1,11.1,9.1,2,11.1,9.1,3,9.1,9.1,\"tank\",1,0,5.1,10000,1,8.299999999999999,5.1,2,13.3,5.1,3,7.1,5.1]]]}");

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
				int x_t, y_t;//t����temp
				x_t = input[i]["final_enemy_positions"][2 * j].asInt();
				y_t = input[i]["final_enemy_positions"][2 * j + 1].asInt();
				if (x_t == -2 and y_t == -2)
				{
					if (enemy_tank[j].in_bush == 0)//��һ�غ���δ�������֣���ô��һ�غ���������λ���ǿ���ȷ����
					{
						int act = input[i]["action"][j].asInt();
						enemy_tank[j].x += px[act];
						enemy_tank[j].y += py[act];
						enemy_tank[j].cal_pos();
						enemy_tank[j].in_bush = 1;
						enemy_tank[j].possible[enemy_tank[j].y][enemy_tank[j].x] = 1;
					}
					else //��һ�غϲ��������ڣ����ݴ����possible����������һ�غϿ����ڵ�����λ��
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
				//enemy_position[j] = input[i]["final_enemy_positions"][j].asInt(), seed += enemy_position[j], seed ^= enemy_position[j];
			}
				
			for (int j = 0; j < input[i]["destroyed_blocks"].size(); j += 2)//���Ը����������з�λ��,��ʱ��û��
			{
				int x = input[i]["destroyed_blocks"][j].asInt();
				int y = input[i]["destroyed_blocks"][j + 1].asInt();
				seed ^= x ^ y;
				state[y][x] = 0;
			}
		}
	}
	
	for (int i = 0; i < 2; i++)//��tank����λ�ø���Ϊ16
	{
		state[self_tank[i].y][self_tank[i].x] = 16;
		if (enemy_tank[i].x >= 0)//û���Ҳ��ڲݴ���
		{
			state[enemy_tank[i].y][enemy_tank[i].x] = 16;
		}
	} 

	//cout << "enemy " << enemy_tank[0].y << ' ' << enemy_tank[0].x << endl << enemy_tank[1].y << ' ' << enemy_tank[1].x << endl;

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
	
	point actions;
	actions = move_generator();
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
	/*	int xx = self_tank[i].x + px[direction];
		int yy = self_tank[i].y + py[direction];
		if (state[yy][xx] & brick)
		{
			if (self_tank[i].shoot_cnt > 0) action = direction + 4;
			else action = -1;
		}
		else action = direction;
		if (!valid(i, action)) action = -1;
		output.append(action);*/
	}
	
	
	output.append(actions.x), output.append(actions.y);output.append(debug);
	Json::FastWriter writer;
	cout << writer.write(output);
//	cout << writer.write(debug);
//	cout << "wrong?" << endl;
	return 0;
}


// ���г���: Ctrl + F5 ����� >����ʼִ��(������)���˵�
// ���Գ���: F5 ����� >����ʼ���ԡ��˵�

// ������ʾ: 
//   1. ʹ�ý��������Դ�������������/�����ļ�
//   2. ʹ���Ŷ���Դ�������������ӵ�Դ�������
//   3. ʹ��������ڲ鿴���������������Ϣ
//   4. ʹ�ô����б��ڲ鿴����
//   5. ת������Ŀ��>���������Դ����µĴ����ļ�����ת������Ŀ��>�����������Խ����д����ļ���ӵ���Ŀ
//   6. ��������Ҫ�ٴδ򿪴���Ŀ����ת�����ļ���>���򿪡�>����Ŀ����ѡ�� .sln �ļ�
