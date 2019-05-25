//CUI版Judge
//注释下行可以显示每轮情况
//#define _SIMPLEOUTPUT_
#include <stack>
#include <set>
#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include <queue>
#include "jsoncpp/json.h"
#include "bot.h"

#define _LOCAL_JUDGE_

using namespace std;
//using namespace Graph_lib;

int tx, ty;

namespace TankGame
{
	using std::stack;
	using std::set;
	using std::istream;

#ifdef _MSC_VER
#pragma region 常量定义和说明
#endif

	enum GameResult
	{
		NotFinished = -2,
		Draw = -1,
		Blue = 0,
		Red = 1
	};

	enum FieldItem
	{
		None = 0,
		Brick = 1,
		Steel = 2,
		Base = 4,
		Blue0 = 8,
		Blue1 = 16,
		Red0 = 32,
		Red1 = 64,
		Water = 128,
		Forest = 256
	};

	template<typename T> inline T operator~ (T a) { return (T)~(int)a; }
	template<typename T> inline T operator| (T a, T b) { return (T)((int)a | (int)b); }
	template<typename T> inline T operator& (T a, T b) { return (T)((int)a & (int)b); }
	template<typename T> inline T operator^ (T a, T b) { return (T)((int)a ^ (int)b); }
	template<typename T> inline T& operator|= (T& a, T b) { return (T&)((int&)a |= (int)b); }
	template<typename T> inline T& operator&= (T& a, T b) { return (T&)((int&)a &= (int)b); }
	template<typename T> inline T& operator^= (T& a, T b) { return (T&)((int&)a ^= (int)b); }

	enum Action
	{
		Invalid = -3,
		Unknown = -2,
		Stay = -1,
		Up, Right, Down, Left,
		UpShoot, RightShoot, DownShoot, LeftShoot
	};

	// 坐标左上角为原点（0, 0），x 轴向右延伸，y 轴向下延伸
	// Side（对战双方） - 0 为蓝，1 为红
	// Tank（每方的坦克） - 0 为 0 号坦克，1 为 1 号坦克
	// Turn（回合编号） - 从 1 开始

	const int fieldHeight = 9, fieldWidth = 9, sideCount = 2, tankPerSide = 2;

	// 基地的横坐标
	const int baseX[sideCount] = { fieldWidth / 2, fieldWidth / 2 };

	// 基地的纵坐标
	const int baseY[sideCount] = { 0, fieldHeight - 1 };

	const int dx[4] = { 0, 1, 0, -1 }, dy[4] = { -1, 0, 1, 0 };
	const FieldItem tankItemTypes[sideCount][tankPerSide] = {
		{ Blue0, Blue1 },{ Red0, Red1 }
	};

	int maxTurn = 100;

#ifdef _MSC_VER
#pragma endregion

#pragma region 工具函数和类
#endif

	inline bool ActionIsMove(Action x)
	{
		return x >= Up && x <= Left;
	}

	inline bool ActionIsShoot(Action x)
	{
		return x >= UpShoot && x <= LeftShoot;
	}

	inline bool ActionDirectionIsOpposite(Action a, Action b)
	{
		return a >= Up && b >= Up && (a + 2) % 4 == b % 4;
	}

	inline bool CoordValid(int x, int y)
	{
		return x >= 0 && x < fieldWidth && y >= 0 && y < fieldHeight;
	}

	inline int count(FieldItem item)
	{
		int cnt = 0;
		while (item)cnt += 1, item = (FieldItem)(((int)item) & (((int)item) - 1));
		return cnt;
	}
	// 判断 item 是不是叠在一起的多个坦克
	inline bool HasMultipleTank(FieldItem item) // changed in tank2s
	{
		int cnt = count(item);
		cnt -= !!(item & Forest);//除去forest 
		return cnt == 2;
		//      return !!(item & (item - 1));
	}

	inline int GetTankSide(FieldItem item)
	{
		return item == Blue0 || item == Blue1 ? Blue : Red;
	}
	inline int GetTankID(FieldItem item)
	{
		return item == Blue0 || item == Red0 ? 0 : 1;
	}

	// 获得动作的方向
	inline int ExtractDirectionFromAction(Action x)
	{
		if (x >= Up)
			return x % 4;
		return -1;
	}

	// 物件消失的记录，用于回退
	struct DisappearLog
	{
		FieldItem item;

		// 导致其消失的回合的编号
		int turn;

		int x, y;
		bool operator< (const DisappearLog& b) const
		{
			if (x == b.x)
			{
				if (y == b.y)
					return item < b.item;
				return y < b.y;
			}
			return x < b.x;
		}
	};

#ifdef _MSC_VER
#pragma endregion

#pragma region TankField 主要逻辑类
#endif

	class TankField
	{
	public:
		//!//!//!// 以下变量设计为只读，不推荐进行修改 //!//!//!//

		// 游戏场地上的物件（一个格子上可能有多个坦克）
		FieldItem gameField[fieldHeight][fieldWidth] = {};

		// 坦克是否存活
		bool tankAlive[sideCount][tankPerSide] = { { true, true },{ true, true } };

		// 基地是否存活
		bool baseAlive[sideCount] = { true, true };

		// 坦克横坐标，-1表示坦克已炸
		int tankX[sideCount][tankPerSide] = {
			{ fieldWidth / 2 - 2, fieldWidth / 2 + 2 },{ fieldWidth / 2 + 2, fieldWidth / 2 - 2 }
		};

		// 坦克纵坐标，-1表示坦克已炸
		int tankY[sideCount][tankPerSide] = { { 0, 0 },{ fieldHeight - 1, fieldHeight - 1 } };

		// 当前回合编号
		int currentTurn = 1;

		// 我是哪一方
		int mySide;

		// 用于回退的log
		stack<DisappearLog> logs;
		set<DisappearLog> last_delete;

		// 过往动作（previousActions[x] 表示所有人在第 x 回合的动作，第 0 回合的动作没有意义）
		Action previousActions[101][sideCount][tankPerSide] = { { { Stay, Stay },{ Stay, Stay } } };

		//!//!//!// 以上变量设计为只读，不推荐进行修改 //!//!//!//

		// 本回合双方即将执行的动作，需要手动填入
		Action nextAction[sideCount][tankPerSide] = { { Invalid, Invalid },{ Invalid, Invalid } };

		// 判断行为是否合法（出界或移动到非空格子算作非法）
		// 未考虑坦克是否存活
		bool ActionIsValid(int side, int tank, Action act)
		{
			if (act == Invalid)
				return false;
			if (act > Left && previousActions[currentTurn - 1][side][tank] > Left) // 连续两回合射击
				return false;
			if (act == Stay || act > Left)
				return true;
			int x = tankX[side][tank] + dx[act],
				y = tankY[side][tank] + dy[act];
			tx = x; ty = y;
			if (!CoordValid(x, y))
				return false;
			if (gameField[y][x] & Forest)
			{
				if (x == tankX[side][tank ^ 1] && y == tankY[side][tank ^ 1])
					return false;
				else return true;
			}
			return gameField[y][x] == None;//!((gameField[y][x] & Steel) || (gameField[y][x] & Brick) || (gameField[y][x] & Water));
										   // water cannot be stepped on but forest can(changed in tank2s)
		}

		// 判断 nextAction 中的所有行为是否都合法
		// 忽略掉未存活的坦克
		bool ActionIsValid()
		{
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
					if (tankAlive[side][tank] && !ActionIsValid(side, tank, nextAction[side][tank]))
						return false;
			return true;
		}

	private:
		void _destroyTank(int side, int tank)
		{
			tankAlive[side][tank] = false;
			tankX[side][tank] = tankY[side][tank] = -1;
		}

		void _revertTank(int side, int tank, DisappearLog& log)
		{
			int &currX = tankX[side][tank], &currY = tankY[side][tank];
			if (tankAlive[side][tank])
				gameField[currY][currX] &= ~tankItemTypes[side][tank];
			else
				tankAlive[side][tank] = true;
			currX = log.x;
			currY = log.y;
			gameField[currY][currX] |= tankItemTypes[side][tank];
		}
	public:

		// 执行 nextAction 中指定的行为并进入下一回合，返回行为是否合法
		bool DoAction()
		{
			if (!ActionIsValid())
				return false;

			// 1 移动
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					Action act = nextAction[side][tank];

					// 保存动作
					previousActions[currentTurn][side][tank] = act;
					if (tankAlive[side][tank] && ActionIsMove(act))
					{
						int &x = tankX[side][tank], &y = tankY[side][tank];
						FieldItem &items = gameField[y][x];

						// 记录 Log
						DisappearLog log;
						log.x = x;
						log.y = y;
						log.item = tankItemTypes[side][tank];
						log.turn = currentTurn;
						logs.push(log);

						// 变更坐标
						x += dx[act];
						y += dy[act];

						// 更换标记（注意格子可能有多个坦克）
						gameField[y][x] |= log.item;
						items &= ~log.item;
					}
				}

			// 2 射♂击!
			set<DisappearLog> itemsToBeDestroyed;
			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					Action act = nextAction[side][tank];
					if (tankAlive[side][tank] && ActionIsShoot(act))
					{
						int dir = ExtractDirectionFromAction(act);
						int x = tankX[side][tank], y = tankY[side][tank];
						bool hasMultipleTankWithMe = HasMultipleTank(gameField[y][x]);
						while (true)
						{
							x += dx[dir];
							y += dy[dir];
							if (!CoordValid(x, y))
								break;
							FieldItem items = gameField[y][x];
							//tank will not be on water, and water will not be shot, so it can be handled as None
							if ((items & Forest) && !((items >> 3) % 16))//continue when meeting empty forest (changed in tank2s)
							{
								continue;
							}
							if (items != None && items != Water)
							{
								// 对射判断
								if (items >= Blue0 &&
									!hasMultipleTankWithMe && !HasMultipleTank(items))
								{
									// 自己这里和射到的目标格子都只有一个坦克
									Action theirAction = nextAction[GetTankSide(items)][GetTankID(items)];
									if (ActionIsShoot(theirAction) &&
										ActionDirectionIsOpposite(act, theirAction))
									{
										// 而且我方和对方的射击方向是反的
										// 那么就忽视这次射击
										break;
									}
								}

								// 标记这些物件要被摧毁了（防止重复摧毁）
								for (int mask = 1; mask <= Red1; mask <<= 1)
									if (items & mask)
									{
										DisappearLog log;
										log.x = x;
										log.y = y;
										log.item = (FieldItem)mask;
										//cout<<"delete in "<<log.x<<' '<<log.y<<' '<<log.item<<endl;
										log.turn = currentTurn;
										itemsToBeDestroyed.insert(log);
									}
								break;
							}
						}
					}
				}
			last_delete = itemsToBeDestroyed;
			for (auto& log : itemsToBeDestroyed)
			{
				switch (log.item)
				{
				case Base:
				{
					int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
					baseAlive[side] = false;
					break;
				}
				case Blue0:
					_destroyTank(Blue, 0);
					break;
				case Blue1:
					_destroyTank(Blue, 1);
					break;
				case Red0:
					_destroyTank(Red, 0);
					break;
				case Red1:
					_destroyTank(Red, 1);
					break;
				case Steel:
					continue;
				default:
					;
				}
				gameField[log.y][log.x] &= ~log.item;
				logs.push(log);
			}

			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
					nextAction[side][tank] = Invalid;

			currentTurn++;
			return true;
		}

		// 回到上一回合
		bool Revert()
		{
			if (currentTurn == 1)
				return false;

			currentTurn--;
			while (!logs.empty())
			{
				DisappearLog& log = logs.top();
				if (log.turn == currentTurn)
				{
					logs.pop();
					switch (log.item)
					{
					case Base:
					{
						int side = log.x == baseX[Blue] && log.y == baseY[Blue] ? Blue : Red;
						baseAlive[side] = true;
						gameField[log.y][log.x] = Base;
						break;
					}
					case Brick:
						gameField[log.y][log.x] = Brick;
						break;
					case Blue0:
						_revertTank(Blue, 0, log);
						break;
					case Blue1:
						_revertTank(Blue, 1, log);
						break;
					case Red0:
						_revertTank(Red, 0, log);
						break;
					case Red1:
						_revertTank(Red, 1, log);
						break;
					default:
						;
					}
				}
				else
					break;
			}
			return true;
		}

		// 游戏是否结束？谁赢了？
		GameResult GetGameResult()
		{

			bool fail[sideCount] = {};
			for (int side = 0; side < sideCount; side++)
				if ((!tankAlive[side][0] && !tankAlive[side][1]) || !baseAlive[side])
					fail[side] = true;
			for (int r = 0; r<2; r++)
			{
				int tx = 4, ty = r * 8;
				for (int i = 0; i<2; i++)
				{
					for (int j = 0; j<2; j++)
					{
						if (tankX[i][j] == tx && tankY[i][j] == ty)
							fail[r] = true;
					}
				}
			}

			if (fail[0] == fail[1])
				return fail[0] || currentTurn > maxTurn ? Draw : NotFinished;
			if (fail[Blue])
				return Red;
			return Blue;
		}

		/* 三个 int 表示场地 01 矩阵（每个 int 用 27 位表示 3 行）
		initialize gameField[][]
		brick>water>steel
		*/
		TankField(int hasBrick[3], int hasWater[3], int hasSteel[3], int hasForest[3], int mySide) : mySide(mySide)
		{
			for (int i = 0; i < 3; i++)
			{
				int mask = 1;
				for (int y = i * 3; y < (i + 1) * 3; y++)
				{
					for (int x = 0; x < fieldWidth; x++)
					{
						if (hasBrick[i] & mask)
							gameField[y][x] = Brick;
						else if (hasWater[i] & mask)
							gameField[y][x] = Water;
						else if (hasSteel[i] & mask)
							gameField[y][x] = Steel;
						else if (hasForest[i] & mask)
							gameField[y][x] = Forest;
						mask <<= 1;
					}
				}
			}
			for (int side = 0; side < sideCount; side++)
			{
				for (int tank = 0; tank < tankPerSide; tank++)
					gameField[tankY[side][tank]][tankX[side][tank]] = tankItemTypes[side][tank];
				gameField[baseY[side]][baseX[side]] = Base;
			}
		}

		// 打印场地
		void DebugPrint()
		{
			const string side2String[] = { "蓝", "红" };
			const string boolean2String[] = { "已炸", "存活" };
			const char* boldHR = "==============================";
			const char* slimHR = "------------------------------";
			cout << boldHR << endl
				<< "图例：" << endl
				<< ". - 空\t# - 砖\t% - 钢\t* - 基地\t@ - 多个坦克" << endl
				<< "b - 蓝0\tB - 蓝1\tr - 红0\tR - 红1\tW - 水\tF - 林" << endl //Tank2 feature
				<< slimHR << endl;
			for (int y = 0; y < fieldHeight; y++)
			{
				for (int x = 0; x < fieldWidth; x++)
				{
					switch (gameField[y][x])
					{
					case None:
						cout << '.';
						break;
					case Brick:
						cout << '#';
						break;
					case Steel:
						cout << '%';
						break;
					case Base:
						cout << '*';
						break;
					case Blue0:
						cout << 'b';
						break;
					case Blue1:
						cout << 'B';
						break;
					case Red0:
						cout << 'r';
						break;
					case Red1:
						cout << 'R';
						break;
					case Water:
						cout << 'W';
						break;
					case Forest:
						cout << 'F';
						break;
					default:
						cout << '@';
						break;
					}
				}
				cout << endl;
			}
			cout << slimHR << endl;
			for (int side = 0; side < sideCount; side++)
			{
				cout << side2String[side] << "：基地" << boolean2String[baseAlive[side]];
				for (int tank = 0; tank < tankPerSide; tank++)
					cout << ", 坦克" << tank << boolean2String[tankAlive[side][tank]];
				cout << endl;
			}
			cout << "当前回合：" << currentTurn << "，";
			GameResult result = GetGameResult();
			if (result == -2)
				cout << "游戏尚未结束" << endl;
			else if (result == -1)
				cout << "游戏平局" << endl;
			else
				cout << side2String[result] << "方胜利" << endl;
			cout << boldHR << endl;
		}

		bool operator!= (const TankField& b) const
		{

			for (int y = 0; y < fieldHeight; y++)
				for (int x = 0; x < fieldWidth; x++)
					if (gameField[y][x] != b.gameField[y][x])
						return true;

			for (int side = 0; side < sideCount; side++)
				for (int tank = 0; tank < tankPerSide; tank++)
				{
					if (tankX[side][tank] != b.tankX[side][tank])
						return true;
					if (tankY[side][tank] != b.tankY[side][tank])
						return true;
					if (tankAlive[side][tank] != b.tankAlive[side][tank])
						return true;
				}

			if (baseAlive[0] != b.baseAlive[0] ||
				baseAlive[1] != b.baseAlive[1])
				return true;

			if (currentTurn != b.currentTurn)
				return true;

			return false;
		}
	};

#ifdef _MSC_VER
#pragma endregion
#endif

	TankField *field = nullptr;

#ifdef _MSC_VER
#pragma region 与平台交互部分
#endif

	// 内部函数
	namespace Internals
	{
		Json::Reader reader;
#ifdef _BOTZONE_ONLINE
		Json::FastWriter writer;
#else
		Json::StyledWriter writer;
#endif

		void _processRequestOrResponse(Json::Value& value, bool isOpponent, bool first)
		{
			if (!first)
			{
				if (!isOpponent)
				{
					for (int tank = 0; tank < tankPerSide; tank++)
						field->nextAction[field->mySide][tank] = (Action)value[tank].asInt();
				}
				else
				{
					for (int tank = 0; tank < tankPerSide; tank++)
					{
						field->nextAction[1 - field->mySide][tank] = (Action)value["action"][tank].asInt();
						if (field->nextAction[1 - field->mySide][tank] == Unknown)
							field->nextAction[1 - field->mySide][tank] = Stay;
					}
					field->DoAction();
				}
			}
			else
			{
				
				// 是第一回合，裁判在介绍场地
				int hasBrick[3], hasWater[3], hasSteel[3], hasForest[3];
				for (int i = 0; i < 3; i++) {//Tank2 feature(???????????????)
					hasWater[i] = value["waterfield"][i].asInt();
					hasBrick[i] = value["brickfield"][i].asInt();
					hasSteel[i] = value["steelfield"][i].asInt();
					hasForest[i] = value["forestfield"][i].asInt();
				}
				if (field != nullptr)delete field;
				field = new TankField(hasBrick, hasWater, hasSteel, hasForest, value["mySide"].asInt());
			}
		}

		// 请使用 SubmitAndExit 或者 SubmitAndDontExit
		void _submitAction(Action tank0, Action tank1, string debug = "", string data = "", string globalData = "")
		{
			Json::Value output(Json::objectValue), response(Json::arrayValue);
			response[0U] = tank0;
			response[1U] = tank1;
			Json::Value a = Json::Value(Json::arrayValue);
			a.append(field->tankX[field->mySide][0]); a.append(field->tankY[field->mySide][0]);
			a.append(field->tankX[field->mySide][1]); a.append(field->tankY[field->mySide][1]);
			output["debug"] = a;
			output["response"] = response;
			if (!debug.empty())
				output["debug"] = debug;
			if (!data.empty())
				output["data"] = data;
			if (!globalData.empty())
				output["globalData"] = globalData;
			cout << writer.write(output) << endl;
		}
	}

	// 从输入流（例如 cin 或者 fstream）读取回合信息，存入 TankField，并提取上回合存储的 data 和 globaldata
	// 本地调试的时候支持多行，但是最后一行需要以没有缩进的一个"}"或"]"结尾
	void ReadInput(istream& in, string& outData, string& outGlobalData)
	{
		Json::Value input;
		string inputString;
		do
		{
			getline(in, inputString);
		} while (inputString.empty());
#ifndef _BOTZONE_ONLINE
		// 猜测是单行还是多行
		char lastChar = inputString[inputString.size() - 1];
		if (lastChar != '}' && lastChar != ']')
		{
			// 第一行不以}或]结尾，猜测是多行
			string newString;
			do
			{
				getline(in, newString);
				inputString += newString;
			} while (newString != "}" && newString != "]");
		}
#endif
		Internals::reader.parse(inputString, input);

		if (input.isObject())
		{
			Json::Value requests = input["requests"], responses = input["responses"];
			if (!requests.isNull() && requests.isArray())
			{
				size_t i, n = requests.size();
				for (i = 0; i < n; i++)
				{
					Internals::_processRequestOrResponse(requests[i], true, !i);
					if (i < n - 1)
						Internals::_processRequestOrResponse(responses[i], false, false);
				}
				outData = input["data"].asString();
				outGlobalData = input["globaldata"].asString();
				return;
			}
		}
		//Internals::_processRequestOrResponse(input, true);
	}

	// 提交决策并退出，下回合时会重新运行程序
	void SubmitAndExit(Action tank0, Action tank1, string debug = "", string data = "", string globalData = "")
	{
		Internals::_submitAction(tank0, tank1, debug, data, globalData);
		exit(0);
	}

	// 提交决策，下回合时程序继续运行（需要在 Botzone 上提交 Bot 时选择“允许长时运行”）
	// 如果游戏结束，程序会被系统杀死
	void SubmitAndDontExit(Action tank0, Action tank1)
	{
		Internals::_submitAction(tank0, tank1);
		field->nextAction[field->mySide][0] = tank0;
		field->nextAction[field->mySide][1] = tank1;
		cout << ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<" << endl;
	}
#ifdef _MSC_VER
#pragma endregion
#endif
}

namespace TankJudge
{
	using namespace TankGame;

	int RandBetween(int from, int to)
	{
		return rand() % (to - from) + from;
	}

	int fieldBinary[3], waterBinary[3], steelBinary[3], forestBinary[3];

	bool visited[fieldHeight][fieldWidth];

	/*****************copied from TankField****************/
	int tankX[sideCount][tankPerSide] = {
		{ fieldWidth / 2 - 2, fieldWidth / 2 + 2 },{ fieldWidth / 2 + 2, fieldWidth / 2 - 2 }
	};
	int tankY[sideCount][tankPerSide] = { { 0, 0 },{ fieldHeight - 1, fieldHeight - 1 } };
	/*****************************************************/

	int dirEnumerateList[][4] = {
		{ Up, Right, Down, Left },
	{ Up, Right, Left, Down },
	{ Up, Down, Right, Left },
	{ Up, Down, Left, Right },
	{ Up, Left, Right, Down },
	{ Up, Left, Down, Right },
	{ Right, Up, Down, Left },
	{ Right, Up, Left, Down },
	{ Right, Down, Up, Left },
	{ Right, Down, Left, Up },
	{ Right, Left, Up, Down },
	{ Right, Left, Down, Up },
	{ Down, Up, Right, Left },
	{ Down, Up, Left, Right },
	{ Down, Right, Up, Left },
	{ Down, Right, Left, Up },
	{ Down, Left, Up, Right },
	{ Down, Left, Right, Up },
	{ Left, Up, Right, Down },
	{ Left, Up, Down, Right },
	{ Left, Right, Up, Down },
	{ Left, Right, Down, Up },
	{ Left, Down, Up, Right },
	{ Left, Down, Right, Up }
	};
	//BFS to ensure that there is only one connected component
	//InitializeField already ensures that water&steel will not appear on base and tank
	struct node { int x, y; node() {}node(int xx, int yy) { x = xx, y = yy; } };
	std::queue<node> q;
	bool EnsureConnected(bool hasWater[fieldHeight][fieldWidth], bool hasSteel[fieldHeight][fieldWidth]) {
		int jishu = 0, jishu2 = 0;
		bool vis[fieldHeight][fieldWidth] = { 0 };
		for (int i = 0; i < fieldHeight; i++)
			for (int j = 0; j < fieldWidth; j++)
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
	//initialize the battlefield before 1st round
	//insert water here
	void color_block(bool a[fieldHeight][fieldWidth], int posy, int posx)
	{
		if (posy >= fieldHeight)return;
		if (posx >= fieldWidth)return;
		a[posy][posx] = true;
	}
	void InitializeField()
	{
		memset(visited, 0, sizeof(visited));
		bool hasBrick[fieldHeight][fieldWidth] = {};
		bool hasWater[fieldHeight][fieldWidth] = {};
		bool hasSteel[fieldHeight][fieldWidth] = {};
		bool hasForest[fieldHeight][fieldWidth] = {};
		int portionH = (fieldHeight + 1) / 2;
		do {//optimistic approach: assume that disconnectivity will not appear normally
			int f_posy = rand() % (portionH - 2);
			int f_posx = rand() % 9;
			//cout<<"Forest "<<f_posx<<' '<<f_posy<<endl;
			for (int y = 0; y <= 4; y++)
				for (int x = 0; x <= 5; x++)
					color_block(hasForest, f_posy + y, f_posx + x);
			/*cout<<"A"<<endl;
			for(int y=0;y<9;y++)
			{
			for(int x=0;x<9;x++)cout<<hasForest[y][x];cout<<endl;
			}*/
			for (int y = 0; y < portionH; y++)
				for (int x = 0; x < fieldWidth; x++) {
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
				for (int x = 0; x < fieldWidth; x++) {
					hasBrick[fieldHeight - y - 1][fieldWidth - x - 1] = hasBrick[y][x];
					hasWater[fieldHeight - y - 1][fieldWidth - x - 1] = hasWater[y][x];
					hasSteel[fieldHeight - y - 1][fieldWidth - x - 1] = hasSteel[y][x];
					hasForest[fieldHeight - y - 1][fieldWidth - x - 1] = hasForest[y][x];
				}
			//separate the field into 4 pieces, each with a tank
			for (int y = 2; y < fieldHeight - 2; y++) {
				hasBrick[y][fieldWidth / 2] = true;
				hasWater[y][fieldWidth / 2] = false;
				hasSteel[y][fieldWidth / 2] = false;
				hasForest[y][fieldWidth / 2] = false;
			}
			for (int x = 0; x < fieldWidth; x++) {
				hasBrick[fieldHeight / 2][x] = true;
				hasWater[fieldHeight / 2][x] = false;
				hasSteel[fieldHeight / 2][x] = false;
				hasForest[fieldHeight / 2][x] = false;
			}
			for (int side = 0; side < sideCount; side++)
			{
				for (int tank = 0; tank < tankPerSide; tank++)
					hasForest[tankY[side][tank]][tankX[side][tank]] = hasSteel[tankY[side][tank]][tankX[side][tank]] = hasWater[tankY[side][tank]][tankX[side][tank]] = false;
				hasForest[baseY[side]][baseX[side]] = hasSteel[baseY[side]][baseX[side]] = hasWater[baseY[side]][baseX[side]] = hasBrick[baseY[side]][baseX[side]] = false;
			}
			//add steel onto midpoint&midtankpoint
			hasBrick[fieldHeight / 2][fieldWidth / 2] = false;
			hasForest[fieldHeight / 2][fieldWidth / 2] = false;
			hasWater[fieldHeight / 2][fieldWidth / 2] = false;
			hasSteel[fieldHeight / 2][fieldWidth / 2] = true;

			for (int tank = 0; tank < tankPerSide; tank++) {
				hasSteel[fieldHeight / 2][tankX[0][tank]] = true;
				hasForest[fieldHeight / 2][tankX[0][tank]] = hasWater[fieldHeight / 2][tankX[0][tank]] = hasBrick[fieldHeight / 2][tankX[0][tank]] = false;
				for (int y = 0; y < portionH; y++)
					for (int x = 0; x < fieldWidth; x++)
						if (hasForest[y][x])hasSteel[y][x] = hasWater[y][x] = hasBrick[y][x] = false;
			}
		} while (!EnsureConnected(hasWater, hasSteel));
		for (int i = 0; i < 3; i++)//3 row as one number
		{
			int mask = 1;
			for (int y = i * 3; y < (i + 1) * 3; y++)
			{
				for (int x = 0; x < fieldWidth; x++)
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
}

int RandBetween(int from, int to)
{
	return rand() % (to - from) + from;
}
TankGame::Action RandAction(int tank)
{
	if (TankGame::field->tankAlive[TankGame::field->mySide][tank] == false)return TankGame::Action::Stay;
	while (true)
	{
		auto act = (TankGame::Action)RandBetween(TankGame::Stay, TankGame::LeftShoot + 1);
		if (TankGame::field->ActionIsValid(TankGame::field->mySide, tank, act))
			return act;
	}
}





struct samplebot {
	const int none = 0, brick = 1, forest = 2, steel = 4, water = 8;
	int state[9][9];
	int enemy_position[4];//x0,y0,x1,y1;
	int  self_position[4];//x0,y0,x1,y1;
	int myside;
	int px[4] = { 0,1,0,-1 };
	int py[4] = { -1,0,1,0 };
	bool ok(int x, int y)// can a tank steps in?
	{
		return x >= 0 && x <= 8 && y >= 0 && y <= 8 && (~state[y][x] & steel) && (~state[y][x] & water) && (~state[y][x] & brick);
	}
	int shoot_cnt[2];
	bool valid(int id, int action)//is tank id's action valid?
	{
		if (self_position[id * 2 + 1] == (myside) * 8)
		{
			if (self_position[id * 2] < 4 && action == 5)return false;
			if (self_position[id * 2] > 4 && action == 7)return false;// prevent to commit suiside
		}
		if (action == -1 || action >= 4)return true;
		int xx = self_position[id * 2] + px[action];
		int yy = self_position[id * 2 + 1] + py[action];
		if (!ok(xx, yy))return false;
		for (int i = 0; i < 2; i++)
		{
			if (enemy_position[i * 2] >= 0)//can not step into a tank's block (although tanks can overlap inadventently)
			{
				if ((xx - enemy_position[i * 2] == 0) && (yy - enemy_position[i * 2 + 1] == 0))
					return false;
			}
			if (self_position[i * 2] >= 0)
			{
				if ((xx - self_position[i * 2] == 0) && (yy - self_position[i * 2 + 1] == 0))
					return false;
			}
		}

		return true;
	}
	void init()
	{
		memset(enemy_position, -1, sizeof(enemy_position));
		memset(state, 0, 81*sizeof(int));
		state[0][4] = state[8][4] = steel;
	}
	Json::Value bot_main(Json::Value all)
	{
		Json::Reader reader;
		Json::Value input, temp, output;

		init();
		int seed = rand();
		input = all["requests"];
		for (int i = 0; i < input.size(); i++)
		{
			if (i == 0)// read in the map information
			{
				myside = input[0u]["mySide"].asInt();
				if (!myside)
				{
					self_position[0] = 2;
					self_position[1] = 0;
					self_position[2] = 6;
					self_position[3] = 0;
				}
				else
				{
					self_position[0] = 6;
					self_position[1] = 8;
					self_position[2] = 2;
					self_position[3] = 8;
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
				for (int j = 0; j < 4; j++)enemy_position[j] = input[i]["final_enemy_positions"][j].asInt(), seed += enemy_position[j], seed ^= enemy_position[j];
				for (int j = 0; j < input[i]["destroyed_blocks"].size(); j += 2)
				{
					int x = input[i]["destroyed_blocks"][j].asInt();
					int y = input[i]["destroyed_blocks"][j + 1].asInt();
					seed ^= x ^ y;
					state[y][x] = 0;
				}
			}
		}


		input = all["responses"];
		for (int i = 0; i < input.size(); i++)//update self state
		{
			for (int j = 0; j < 2; j++)
			{
				int x = input[i][j].asInt();
				seed ^= x * 2354453456;
				if (x >= 4)shoot_cnt[j] = 1;
				else shoot_cnt[j] = 0;
				if (x == -1 || x >= 4)continue;
				self_position[2 * j] += px[x];
				self_position[2 * j + 1] += py[x];
			}
		}
		output = Json::Value(Json::arrayValue);
		srand(self_position[0] ^ self_position[2] ^ enemy_position[1] ^ enemy_position[3] ^ seed ^ 565646435);
		int a[2];
		for (int i = 0; i < 2; i++)// sample valid action for 2 tanks
		{
			int action;
			do {
				action = rand() % 9 - 1;
			} while ((!valid(i, action)) || (!!(action >= 4)) + shoot_cnt[i] >= 2);
			a[i] = action;
			output.append(action);
		}
		return output;
	}
};


namespace judge {
	int Bot1 = 0, Bot2 = 0, NoWinner = 0;

	Json::Value judge_main(Json::Value input)
	{
		unsigned int seed;
		const string int2str[] = { "0", "1" };

		Json::Reader reader;
		Json::Value temp, output;
#ifdef _BOTZONE_ONLINE
		reader.parse(cin, input);
#else
		//char s[] = "{\"log\":[{\"keep_running\":false,\"memory\":160,\"output\":{\"command\":\"request\",\"content\":{\"0\":{\"brickfield\":[71620266,4718352,44783889],\"waterfield\":[0,0,0],\"steelfield\":[0,0,0],\"mySide\":0},\"1\":{\"brickfield\":[71620266,4718352,44783889],\"waterfield\":[0,0,0],\"steelfield\":[0,0,0],\"mySide\":1}},\"display\":[71620266,4718352,44783889]},\"time\":3,\"verdict\":\"OK\"},{\"0\":{\"keep_running\":true,\"memory\":165,\"time\":89,\"verdict\":\"OK\",\"debug\":\"DEBUG!\",\"response\":[6,7]},\"1\":{\"keep_running\":true,\"memory\":20,\"time\":4,\"verdict\":\"OK\",\"response\":[0,4]}},{\"keep_running\":false,\"memory\":15,\"output\":{\"command\":\"request\",\"content\":{\"0\":[0,4],\"1\":[6,7]},\"display\":{\"0\":[6,7],\"1\":[0,4]}},\"time\":3,\"verdict\":\"OK\"},{\"0\":{\"keep_running\":true,\"memory\":165,\"time\":1,\"verdict\":\"OK\",\"debug\":\"DEBUG!\",\"response\":[2,-1]},\"1\":{\"keep_running\":true,\"memory\":20,\"time\":0,\"verdict\":\"OK\",\"response\":[0,0]}},{\"keep_running\":false,\"memory\":15,\"output\":{\"command\":\"request\",\"content\":{\"0\":[0,0],\"1\":[2,-1]},\"display\":{\"0\":[2,-1],\"1\":[0,0]}},\"time\":2,\"verdict\":\"OK\"},{\"0\":{\"keep_running\":true,\"memory\":165,\"time\":0,\"verdict\":\"OK\",\"debug\":\"DEBUG!\",\"response\":[0,7]},\"1\":{\"keep_running\":true,\"memory\":20,\"time\":0,\"verdict\":\"OK\",\"response\":[7,0]}}],\"initdata\":{\"waterfield\":[0,0,0],\"steelfield\":[0,0,0],\"maxTurn\":100,\"seed\":152650152}}";
		

#endif
		Json::Value initdata = input["initdata"];

		if (initdata.isString())
			reader.parse(initdata.asString(), initdata);
		if (initdata.isString())
			initdata = Json::Value(Json::objectValue);

		temp = initdata["maxTurn"];
		if (temp.isInt())
			TankGame::maxTurn = temp.asUInt();
		initdata["maxTurn"] = TankGame::maxTurn;

		temp = initdata["seed"];
		if (temp.isInt())
			srand(seed = temp.asUInt());
		else
			srand(seed = time(nullptr));
		initdata["seed"] = seed;

		temp = initdata["brickfield"];
		if (temp.isArray() && !temp.isNull()) {
			for (int i = 0; i < 3; i++)
				TankJudge::fieldBinary[i] = temp[i].asInt();
			initdata["brickfield"] = temp;
			temp = initdata["waterfield"];
			for (int i = 0; i < 3; i++)
				TankJudge::waterBinary[i] = temp[i].asInt();
			initdata["waterfield"] = temp;
			temp = initdata["steelfield"];
			for (int i = 0; i < 3; i++)
				TankJudge::steelBinary[i] = temp[i].asInt();
			initdata["steelfield"] = temp;
			temp = initdata["forestfield"];
			for (int i = 0; i < 3; i++)
				TankJudge::forestBinary[i] = temp[i].asInt();
			initdata["forestfield"] = temp;
		}
		else
		{
			TankJudge::InitializeField();
			temp = Json::Value(Json::arrayValue);
			for (int i = 0; i < 3; i++)
				temp[i] = TankJudge::fieldBinary[i];
			initdata["brickfield"] = temp;
			temp = Json::Value(Json::arrayValue);
			for (int i = 0; i < 3; i++)
				temp[i] = TankJudge::waterBinary[i];
			initdata["waterfield"] = temp;
			temp = Json::Value(Json::arrayValue);
			for (int i = 0; i < 3; i++)
				temp[i] = TankJudge::steelBinary[i];
			initdata["steelfield"] = temp;
			temp = Json::Value(Json::arrayValue);
			for (int i = 0; i < 3; i++)
				temp[i] = TankJudge::forestBinary[i];
			initdata["forestfield"] = temp;
		}

		if (TankGame::field != nullptr) {
			delete TankGame::field;
			TankGame::field = nullptr;
		}
		TankGame::field = new TankGame::TankField(TankJudge::fieldBinary, TankJudge::waterBinary, TankJudge::steelBinary, TankJudge::forestBinary, 0);

		input = input["log"];

		int size = input.size();
		if (size == 0)//before 1st round
		{
			//cout << "对局初始化\n";
			for (int side = 0; side < TankGame::sideCount; side++)
			{
				auto obj = Json::Value(Json::objectValue);
				obj["brickfield"] = initdata["brickfield"];
				obj["waterfield"] = initdata["waterfield"];
				obj["steelfield"] = initdata["steelfield"];
				obj["forestfield"] = initdata["forestfield"];
				obj["mySide"] = side;
				output["content"][int2str[side]] = obj;
			}

			output["initdata"] = initdata;
			output["command"] = "request";
			//display has brick,water and steel
			output["display"]["brick"] = initdata["brickfield"];
			output["display"]["water"] = initdata["waterfield"];
			output["display"]["steel"] = initdata["steelfield"];
			output["display"]["forest"] = initdata["forestfield"];
		}
		else
		{
			//cout<<"beginning"<<endl;
			
			bool invalid[TankGame::sideCount] = {};
			//winning side {0,1}
			auto setWinner = [&](int to) {
				if (to == -1)
					output["content"]["0"] = output["content"]["1"] = 1;
				else if (to == 1)
				{
					output["content"]["0"] = 0;
					output["content"]["1"] = 2;
				}
				else
				{
					output["content"]["0"] = 2;
					output["content"]["1"] = 0;
				}
			};
			//        size -= 4;
			for (int i = 1; i < size; i += 2)
			{
				bool isLast = size - 1 == i;
				Json::Value response = input[i];
				for (int side = 0; side < TankGame::sideCount; side++)//simulate each round
				{
					Json::Value raw = response[int2str[side]],
						answer = raw["response"].isNull() ? raw["content"] : raw["response"];
					TankGame::Action act0, act1;
					if (answer.isArray() && answer[0U].isInt() && answer[1U].isInt())
					{
						act0 = (TankGame::Action)answer[0U].asInt();
						act1 = (TankGame::Action)answer[1U].asInt();
						if (isLast)
						{
							auto action = Json::Value(Json::arrayValue);
							action[0U] = act0;
							action[1U] = act1;
							output["display"][int2str[side]]["action"] = output["content"][int2str[1 - side]]["action"] = action;
							if (!TankGame::field->tankAlive[side][0] || !TankGame::field->ActionIsValid(side, 0, act0))
								output["content"][int2str[1 - side]]["action"][0U] = -1;
							if (!TankGame::field->tankAlive[side][1] || !TankGame::field->ActionIsValid(side, 1, act1))
								output["content"][int2str[1 - side]]["action"][1U] = -1;
						}
						if ((!TankGame::field->tankAlive[side][0] || TankGame::field->ActionIsValid(side, 0, act0)) &&
							(!TankGame::field->tankAlive[side][1] || TankGame::field->ActionIsValid(side, 1, act1)))
						{
							TankGame::field->nextAction[side][0] = act0;
							TankGame::field->nextAction[side][1] = act1;
							continue;
						}
					}
					invalid[side] = true;
					output["debug"] = Json::Value(Json::arrayValue);
					output["debug"].append(tx);
					output["debug"].append(ty);
					output["debug"].append((int)TankGame::field->gameField[ty][tx]);
					output["display"]["loseReason"][side] = "INVALID_INPUT_VERDICT_" + raw["verdict"].asString();
				}
				if (invalid[0] || invalid[1])
				{
					output["command"] = "finish";
					if (invalid[0] == invalid[1])
						setWinner(-1);
					else if (invalid[0])
						setWinner(1);
					else
						setWinner(0);
					break;
				}
				else
				{
					for (int j = 0; j < TankGame::sideCount; j++)
						for (int k = 0; k < TankGame::tankPerSide; k++)
						{
							TankGame::FieldItem items = TankGame::field->gameField[TankGame::field->tankY[j][k]][TankGame::field->tankX[j][k]];
							if (items&TankGame::Forest)
								output["content"][int2str[1 - j]]["action"][k] = (int)(TankGame::Unknown);//森林里发起动作另一bot对该bot动作不可见 
						}
					TankGame::field->DoAction();
					for (int j = 0; j < TankGame::sideCount; j++)
					{
						output["content"][int2str[j]]["destroyed_blocks"] = Json::Value(Json::arrayValue);
						output["content"][int2str[j]]["destroyed_tanks"] = Json::Value(Json::arrayValue);
						for (auto& log : TankGame::field->last_delete)
						{
							if (log.item & TankGame::Brick)
							{
								output["content"][int2str[j]]["destroyed_blocks"].append(log.x);
								output["content"][int2str[j]]["destroyed_blocks"].append(log.y);
							}
							int temp = (int)log.item;
							temp %= 128; temp /= 8;
							if (temp)
							{
								for (int j = 0; j <= 1; j++)
									if (log.x == TankGame::field->tankX[TankGame::field->mySide][j] &&
										log.y == TankGame::field->tankY[TankGame::field->mySide][j])
										TankGame::field->tankAlive[TankGame::field->mySide][j] = false;
								output["content"][int2str[j]]["destroyed_tanks"].append(log.x);
								output["content"][int2str[j]]["destroyed_tanks"].append(log.y);
							}
						}
					}
					for (int j = 0; j < TankGame::sideCount; j++)
					{
						auto temp = Json::Value(Json::arrayValue);
						output["content"][int2str[1 - j]]["final_enemy_positions"] = temp;
						for (int k = 0; k < TankGame::tankPerSide; k++)
						{
							if (!TankGame::field->tankAlive[1 - j][k])
							{
								output["content"][int2str[1 - j]]["final_enemy_positions"].append(-1),
									output["content"][int2str[1 - j]]["final_enemy_positions"].append(-1);
							}
							else
							{
								TankGame::FieldItem items = TankGame::field->gameField[TankGame::field->tankY[j][k]][TankGame::field->tankX[j][k]];
								if (items&TankGame::Forest)
									output["content"][int2str[1 - j]]["final_enemy_positions"].append(-2),
									output["content"][int2str[1 - j]]["final_enemy_positions"].append(-2);
								else
									output["content"][int2str[1 - j]]["final_enemy_positions"].append(TankGame::field->tankX[j][k]),
									output["content"][int2str[1 - j]]["final_enemy_positions"].append(TankGame::field->tankY[j][k]);
							}
						}

					}

				}

				int result = TankGame::field->GetGameResult();
				if (result != -2)
				{
					output["command"] = "finish";
					if (result == 0)
					{
						cout << "Bot1 Win!\n";
						judge::Bot1++;
					}
					else if (result == 1) {
						cout << "Bot2 Win!\n";
						judge::Bot2++;
					}
					else {
						cout << "No Winner!\n";
						judge::NoWinner++;
					}
					setWinner(result);
					for (int side = 0; side < TankGame::sideCount; side++)
					{
						bool tankExist = TankGame::field->tankAlive[side][0] || TankGame::field->tankAlive[side][1];
						bool baseExist = TankGame::field->baseAlive[side];
						if (!tankExist && !baseExist)
							output["display"]["loseReason"][side] = "BASE_TANK_ALL_DESTROYED";
						else if (!tankExist)
							output["display"]["loseReason"][side] = "TANK_ALL_DESTROYED";
						else if (!baseExist)
							output["display"]["loseReason"][side] = "BASE_DESTROYED";
					}
					
#ifdef _SIMPLEOUTPUT_
					TankGame::field->DebugPrint();
#endif // _SIMPLEOUTPUT
					break;
				}
				else if (isLast)
					output["command"] = "request";
			}
		}
#ifndef _SIMPLEOUTPUT_
		TankGame::field->DebugPrint();
#endif // _SIMPLEOUTPUT
		return output;
	}
}
//Platform simulation
int main() {

	string action_name[9] = { "停止","向上","向右","向下","向左","上射","右射" ,"下射" ,"左射" };

	Json::Value judgeinfo;
	bool problem = false;
	for (size_t i = 0; i < 1; i++)
	{
		cout << "Round " << i << " start.\n";
		Json::Reader reader;
		Json::FastWriter writer;
		Json::Value toJudge, resJudge, toBot0, toBot1, outJudge, outBot;
		char s[] = "{\"log\":[],\"initdata\":{\"brickfield\":[4223016,4412944,10514448],\"forestfield\":[129994243,129761775,100896749],\"maxTurn\":100,\"seed\":1558167876,\"steelfield\":[0,43008,2],\"waterfield\":[128,0,524288]}}";

		if(!problem)reader.parse(s, toJudge);
		else toJudge = judgeinfo;
		

		resJudge = judge::judge_main(toJudge);

		if(!problem)judgeinfo = resJudge;
		outJudge["output"] = resJudge;
		toJudge["initdata"] = resJudge["initdata"];
		toBot0["requests"].append(resJudge["content"]["0"]);
		toBot1["requests"].append(resJudge["content"]["1"]);
	
		samplebot bot2;

		while (resJudge["command"].asString() != "finish") {
			srand(time(0) + clock());
			Bot bot1(toBot0);
			//把bot的main()改名为bot_main()，装入bot名称空间，替换掉上述样例名称空间，就能实现本地裁判
			//裁判为标准Tank2S裁判，每轮都先复原局面再判断胜负，所以随轮数增加裁判越来越慢。
			//主要时间消耗在cout，如果去掉局面输出，效率影响不大。
			//但这个版本不能用于训练神经网络，因为裁判效率太低了。	
			outBot["0"]["response"] = bot1.bot_main();
			outBot["1"]["response"] = bot2.bot_main(toBot1);
			toJudge["log"].append(outJudge);
			toJudge["log"].append(outBot);
#ifndef _SIMPLEOUTPUT_
			cout << "Bot0 tank1: " << action_name[outBot["0"]["response"][0].asInt() + 1];
			cout << "\tBot0 tank2: " << action_name[outBot["0"]["response"][1].asInt() + 1];
			cout << "\nBot1 tank1: " << action_name[outBot["1"]["response"][0].asInt() + 1];
			cout << "\tBot1 tank2: " << action_name[outBot["1"]["response"][1].asInt() + 1];
			cout << "\n";
#endif
			//取消注释后可以逐轮输出
			//system("pause");
			resJudge = judge::judge_main(toJudge);
			toBot0["requests"].append(resJudge["content"]["0"]);
			toBot1["requests"].append(resJudge["content"]["1"]);
			toBot0["responses"].append(outBot["0"]["response"]);
			toBot1["responses"].append(outBot["1"]["response"]);
		}
		if (judge::Bot1 == 0&& judge::Bot2==0) {
			cout << "终盘出现问题，请开始调试\n";
			problem = true;
			system("pause");
		}
		judge::Bot1 = 0; judge::Bot2 = 0;
	}
	cout << "Simulation ended.\n";
	cout << "Bot1 win: " << judge::Bot1 << " , Bot2 win: " << judge::Bot2 << " . No winner: " << judge::NoWinner<<".";
	system("pause");
	
	return 0;
}