import numpy as np
import geatpy as ga
import subprocess,time,sys,math
from multiprocessing import Pool,freeze_support
from tqdm import tqdm

# 调试用变量
DEBUG = True
WRITE_TO_FILE = True

# 全局变量
GENERATION = 0
TOTAL = 0
BATTLECOUNTER = 0

# 本程序依赖Tank2Judge.exe
# Tank2Judge.exe 输入表现型矩阵
# 1 3 4 5 6 7 4 7 4 6 7 8 4 8
# 输出十张地图对局中双方的得分(输0 平1 赢2)
# 2 2

# modified
def sga_real_templet(AIM_M, AIM_F, PUN_M, PUN_F, FieldDR, problem, maxormin, MAXGEN, NIND, SUBPOP, GGAP, selectStyle, recombinStyle, recopt, pm, distribute, drawing = 1, _init = None, _read_from_file = False):
    
    """
sga_real_templet.py - 单目标编程模板(实值编码)

语法：
    该函数除了参数drawing外，不设置可缺省参数。当某个参数需要缺省时，在调用函数时传入None即可。
    比如当没有罚函数时，则在调用编程模板时将第3、4个参数设置为None即可，如：
    sga_real_templet(AIM_M, 'aimfuc', None, None, ..., maxormin)

输入参数：
    AIM_M - 目标函数的地址，由AIM_M = __import__('目标函数所在文件名')语句得到
            目标函数规范定义：[f,LegV] = aimfuc(Phen,LegV)
            其中Phen是种群的表现型矩阵, LegV为种群的可行性列向量,f为种群的目标函数值矩阵
    
    AIM_F : str - 目标函数名
    
    PUN_M - 罚函数的地址，由PUN_M = __import__('罚函数所在文件名')语句得到
            罚函数规范定义： newFitnV = punishing(LegV, FitnV)
            其中LegV为种群的可行性列向量, FitnV为种群个体适应度列向量
            一般在罚函数中对LegV为0的个体进行适应度惩罚，返回修改后的适应度列向量newFitnV
    
    PUN_F : str - 罚函数名
    
    FieldDR : array - 实际值种群区域描述器
        [lb;		(float) 指明每个变量使用的下界
         ub]		(float) 指明每个变量使用的上界
         注：不需要考虑是否包含变量的边界值。在crtfld中已经将是否包含边界值进行了处理
         本函数生成的矩阵的元素值在FieldDR的[下界, 上界)之间
    
    problem : str - 表明是整数问题还是实数问题，'I'表示是整数问题，'R'表示是实数问题                 
    
    maxormin int - 最小最大化标记，1表示目标函数最小化；-1表示目标函数最大化
    
    MAXGEN : int - 最大遗传代数
    
    NIND : int - 种群规模，即种群中包含多少个个体
    
    SUBPOP : int - 子种群数量，即对一个种群划分多少个子种群
    
    GGAP : float - 代沟，表示子代与父代染色体及性状不相同的概率
    
    selectStyle : str - 指代所采用的低级选择算子的名称，如'rws'(轮盘赌选择算子)
    
    recombinStyle: str - 指代所采用的低级重组算子的名称，如'xovsp'(单点交叉)
    
    recopt : float - 交叉概率
    
    pm : float - 重组概率
    
    distribute : bool - 是否增强种群的分布性（可能会造成收敛慢）
    
    drawing : int - (可选参数)，0表示不绘图，1表示绘制最终结果图。默认drawing为1

输出参数：
    pop_trace : array - 种群进化记录器(进化追踪器),
                        第0列记录着各代种群最优个体的目标函数值
                        第1列记录着各代种群的适应度均值
                        第2列记录着各代种群最优个体的适应度值
    
    var_trace : array - 变量记录器，记录着各代种群最优个体的变量值，每一列对应一个控制变量
    
    times     : float - 进化所用时间

模板使用注意：
    1.本模板调用的目标函数形如：[ObjV,LegV] = aimfuc(Phen,LegV), 
      其中Phen表示种群的表现型矩阵, LegV为种群的可行性列向量(详见Geatpy数据结构)
    2.本模板调用的罚函数形如: newFitnV = punishing(LegV, FitnV), 
      其中FitnV为用其他算法求得的适应度
    若不符合上述规范，则请修改算法模板或自定义新算法模板
    3.关于'maxormin': geatpy的内核函数全是遵循“最小化目标”的约定的，即目标函数值越小越好。
      当需要优化最大化的目标时，需要设置'maxormin'为-1。
      本算法模板是正确使用'maxormin'的典型范例，其具体用法如下：
      当调用的函数传入参数包含与“目标函数值矩阵”有关的参数(如ObjV,ObjVSel,NDSetObjV等)时，
      查看该函数的参考资料(可用'help'命令查看，也可到官网上查看相应的教程)，
      里面若要求传入前对参数乘上'maxormin',则需要乘上。
      里面若要求对返回参数乘上'maxormin'进行还原，
      则调用函数返回得到的相应参数需要乘上'maxormin'进行还原，否则其正负号就会被改变。

CHANGE:
    1._init为参数初始值(可选):若init为None，则所有Chrom均随机生成，否则则将init添加至初始Chrom
    2.修改了重插入部分
    3.允许通过read_from_file保存种群
"""
    """==========================初始化配置==========================="""
    # 获取目标函数和罚函数
    aimfuc = getattr(AIM_M, AIM_F) # 获得目标函数
    if PUN_F is not None:
        punishing = getattr(PUN_M, PUN_F) # 获得罚函数
    NVAR = FieldDR.shape[1] # 得到控制变量的个数
    # 定义进化记录器，初始值为nan
    pop_trace = (np.zeros((MAXGEN ,2)) * np.nan)
    # 定义变量记录器，记录控制变量值，初始值为nan
    var_trace = (np.zeros((MAXGEN ,NVAR)) * np.nan) 
    ax = None # 存储上一帧图形
    repnum = 0 # 初始化重复个体数为0
    """=========================开始遗传算法进化======================="""
    if problem == 'R':
        if _read_from_file is True:
            Chrom = np.loadtxt('chrom')
        elif _init is None:
            Chrom = ga.crtrp(NIND, FieldDR) # 生成初始种群
        else:
            Chrom = np.vstack([ga.crtrp(NIND-1, FieldDR),_init])
    elif problem == 'I':
        if _read_from_file is True:
            Chrom = np.loadtxt('chrom')
        elif _init is None:
            Chrom = ga.crtip(NIND, FieldDR) # 生成初始种群
        else:
            Chrom = np.vstack([ga.crtip(NIND-1, FieldDR),_init])
    LegV = np.ones((NIND, 1)) # 初始化种群的可行性列向量
    [ObjV, LegV] = aimfuc(Chrom, LegV) # 求种群的目标函数值
    gen = 0
    badCounter = 0 # 用于记录在“遗忘策略下”被忽略的代数
    # 开始进化！！
    start_time = time.time() # 开始计时
    while gen < MAXGEN:
        if badCounter >= 10 * MAXGEN: # 若多花了10倍的迭代次数仍没有可行解出现，则跳出
            break # CHANGE
        FitnV = ga.powing(maxormin * ObjV, LegV, None, SUBPOP) # CHANGE
        if PUN_F is not None:
            FitnV = punishing(LegV, FitnV) # 调用罚函数
        # 记录进化过程
        bestIdx = np.argmax(FitnV) # 获取最优个体的下标
        if LegV[bestIdx] != 0:
            feasible = np.where(LegV != 0)[0] # 排除非可行解
            pop_trace[gen,0] = np.sum(ObjV[feasible]) / ObjV[feasible].shape[0] # 记录种群个体平均目标函数值
            pop_trace[gen,1] = ObjV[bestIdx] # 记录当代目标函数的最优值
            var_trace[gen,:] = Chrom[bestIdx, :] # 记录当代最优的控制变量值
            repnum = len(np.where(ObjV[np.argmax(FitnV)] == ObjV)[0]) # 计算最优个体重复数
            # 绘制动态图
            if drawing == 2:
                ax = ga.sgaplot(pop_trace[:,[1]],'种群最优个体目标函数值', False, ax, gen)
            badCounter = 0 # badCounter计数器清零
            if WRITE_TO_FILE is True:
                np.savetxt('chrom',Chrom)
                with open('temp.txt', 'a') as ftemp:
                    ftemp.write('>>> Generation '+ str(gen) +'\n')
                    ftemp.write('Best Value = ' + str(ObjV[bestIdx])+'\n')
                    ftemp.write('Best Chrom = ')
                    ftemp.writelines(np.array2string(Chrom[bestIdx, :], separator=','))
                    ftemp.write('\n')
                    ftemp.close()
        else:
            gen -= 1 # 忽略这一代（遗忘策略）
            badCounter += 1
        if distribute == True: # 若要增强种群的分布性（可能会造成收敛慢）
            idx = np.argsort(ObjV[:, 0], 0)
            dis = np.diff(ObjV[idx,0]) / (np.max(ObjV[idx,0]) - np.min(ObjV[idx,0]) + 1)# 差分计算距离的修正偏移量
            dis = np.hstack([dis, dis[-1]])
            dis = dis + np.min(dis) # 修正偏移量+最小量=修正绝对量
            FitnV[idx, 0] *= np.exp(dis) # 根据相邻距离修改适应度，突出相邻距离大的个体，以增加种群的多样性
        # 进行遗传算子
        SelCh=ga.selecting(selectStyle, Chrom, FitnV, GGAP, SUBPOP) # 选择
        SelCh=ga.recombin(recombinStyle, SelCh, recopt, SUBPOP) # 对所选个体进行重组
        if problem == 'R':
            SelCh=ga.mutbga(SelCh,FieldDR, pm) # 变异
            if repnum > Chrom.shape[0] * 0.01: # 当最优个体重复率高达1%时，进行一次高斯变异
                SelCh=ga.mutgau(SelCh, FieldDR, pm) # 高斯变异
        elif problem == 'I':
            SelCh=ga.mutint(SelCh, FieldDR, pm)
        LegVSel = np.ones((SelCh.shape[0], 1)) # 初始化育种种群的可行性列向量
        # CHANGE
        currCh = np.vstack([Chrom,SelCh])
        currLegV = np.vstack([LegV,LegVSel])
        [resObjV, resLegV] = aimfuc(currCh, currLegV) # 求种群的目标函数值
        [ObjV,ObjVSel] = np.split(resObjV,[len(ObjV)])
        resFitnV = ga.ranking(maxormin * resObjV, resLegV, None, SUBPOP) # 计算育种种群的适应度
        [FitnV,FitnVSel] = np.split(resFitnV,[len(FitnV)])
        if PUN_F is not None:
            FitnVSel = punishing(LegVSel, FitnVSel) # 调用罚函数
        # 重插入
        [Chrom, ObjV, LegV] = ga.reins(Chrom, SelCh, SUBPOP, 1, 1, FitnV, FitnVSel, ObjV, ObjVSel, LegV, LegVSel)
        gen += 1
        if DEBUG is True:
            for i in range(len(ObjV)):
                print('aimfunc =', ObjV[i], end = ' ')
                print('Chrom =', Chrom[i])
    end_time = time.time() # 结束计时
    times = end_time - start_time
    # 后处理进化记录器
    delIdx = np.where(np.isnan(pop_trace))[0]
    pop_trace = np.delete(pop_trace, delIdx, 0)
    var_trace = np.delete(var_trace, delIdx, 0)
    if pop_trace.shape[0] == 0:
        raise RuntimeError('error: no feasible solution. (有效进化代数为0，没找到可行解。)')
    # 绘图
    if drawing != 0:
        ga.trcplot(pop_trace, [['种群个体平均目标函数值', '种群最优个体目标函数值']])
    # 输出结果
    if maxormin == 1:
        best_gen = np.argmin(pop_trace[MAXGEN - 1, 1]) # 记录最优种群是在哪一代
        best_ObjV = np.min(pop_trace[MAXGEN - 1, 1])
    elif maxormin == -1:
        best_gen = MAXGEN - 1 # 记录最优种群是在哪一代
        best_ObjV = np.max(pop_trace[MAXGEN - 1, 1])
    print('最优的目标函数值为：%s'%(best_ObjV))
    print('最优的控制变量值为：')
    for i in range(NVAR):
        print(var_trace[MAXGEN - 1, i], end = ',')
    print('有效进化代数：%s'%(pop_trace.shape[0]))
    print('最优的一代是第 %s 代'%(best_gen+1))
    print('时间已过 %s 秒'%(times))
    # 返回进化记录器、变量记录器以及执行时间
    ftemp.close()
    return [pop_trace, var_trace, times]

def battle(_para): # para_A,para_B均为单行向量 返回值为十局中得分(List)
	args = ''
	for it in _para[0]:
		args += ' ' + str(it)
	for it in _para[1]:
		args += ' ' + str(it)
	# if DEBUG is True:
	# 	print("[DEBUG] Judge arguments =",args)
	curr_input = args.encode('utf-8')
	curr_process = subprocess.run('190522_Tank2S_Judge.exe', stdout=subprocess.PIPE, input=curr_input) 
	curr_output = curr_process.stdout.decode('utf-8')  # bytes to string
	if DEBUG is True:
	    print("[DEBUG] Judge output = ",curr_output)
	curr_answ = curr_output.split()
	res = [float(curr_answ[0]), float(curr_answ[1])]
	return res

def aimfunc(Phen,LegV):  # 以十局平均得分作为目标函数值
	global GENERATION,TOTAL
	row_count = Phen.shape[0]
	print('Generation',GENERATION)
	_pool = Pool()
	_battle_list = []
	for i in range(row_count-1):  # 单循环赛
		for j in range(i+1,row_count):
			_battle_list.append([Phen[i,:],Phen[j,:]])
	# if(DEBUG):
	# 	print('BattleList:', _battle_list)
	_battle_res = list(tqdm(_pool.imap(battle,_battle_list),total = len(_battle_list),unit = 'battle'))
	counter = 0 
	f = np.zeros((row_count,1))
	for i in range(row_count-1):  # 单循环赛
		for j in range(i+1,row_count):
			f[i] += _battle_res[counter][0]
			f[j] += _battle_res[counter][1]
			counter+=1
	f /= row_count
	GENERATION += 1
	print('BattleRes:',f.T)
	return [f,LegV]

if __name__ == '__main__':
    # For Windows Platform
    freeze_support()

    # 获取函数接口地址
    AIM_M = __import__('train')

    # 调试变量
    parameter_count = 6  # 需要训练的参数数量
    init = [2,1.2,0.3,2.0,5,0.5] #  参数初始值
    upper_bound = np.array([5,2.4,0.6,5,10,1.2])
    lower_bound = np.array([0.5,0.6,0.1,1,2.5,0.2])
    
    # 遗传算法参数
    MAXGEN = 50  # 最大遗传代数
    NIND = 10 # 种群规模 (必须是子种群数量的N倍)
    SUBPOP = 1  # 子种群数量
    GGAP = 0.5 # 代沟，本模板中该参数为无用参数
    selectStyle = 'etour'  # 低级选择算子
    recombinStyle = 'xovsp'  # 低级重组算子
    recopt = 0.9  # 交叉概率
    pm = 0.1  # 重组概率
    distribute = False  # 是否增强种群的分布性（可能会造成收敛慢）

    # 初始化变量
    ranges = np.vstack([lower_bound*np.ones((1,parameter_count)), upper_bound*np.ones((1,parameter_count))]) # CHANGE
    borders = np.vstack([np.zeros((1,parameter_count)), np.zeros((1,parameter_count))])  # bu包含边界
    precisions = [3] * parameter_count  # 在二进制/格雷码编码中代表自变量的编码精度，当控制变量是连续型时，根据crtfld参考资料，该变量只表示边界精度，故设置为一定的正数即可
    
	# 生成区域描述器
    FieldDR = ga.crtfld(ranges, borders, precisions)
    if DEBUG is True:
        print('FieldDR :', FieldDR)

    # 多种群竞争进化 实值编码 最大化目标
    [pop_trace, var_trace, times] = sga_real_templet(AIM_M, 'aimfunc', None, None, FieldDR, 'R', -1, MAXGEN, NIND, SUBPOP,  GGAP,  selectStyle,  recombinStyle, recopt,  pm,  distribute,  1, _init=init)