# train.py 使用指南

emm,你说的是,可我怎么训呢？

## 依赖环境

由于我并没有什么耐心(水平)做自己的实现,因此用到了不少现成的轮子:

1.  geatpy (截止本文档写成之时,geatpy似乎只能支持windows)

2.  numpy

3.  tqdm (用于显示进度条)

4.  您的裁判程序

*仅在Windows 10, python 3.7.1, Anaconda环境下跑过测试

## Quick Start

由于我并没有什么能力把它做的好用一些, 为了让它能跑起来, 您需要修改以下文件:

1.  train.py

        需要训练的参数数量  parameter_count = 5  (例)

        参数初始值  init = [1.2,0.3,2.0,5,0.5] (例)

        参数上界    upper_bound = np.array([2.4,0.6,5,10,1.2]) (例)
    
        参数下界    lower_bound = np.array([0.6,0.1,1,2.5,0.2]) (例)

        程序路径    您可以将自己的裁判程序(可执行文件)改名为Judge.exe放入同目录, 或将第240行中的'Judge.exe'修改为自己的可执行文件路径

2.  裁判程序

    裁判程序的输入为两组参数, 由stdin读入   1.0 2.0 3.1 4.2 5.3 4.4 (参数数量为3时的示例)
    
    输出为应用参数1, 2的bot在一定数量对局后的总得分, 由stdout输出   15 18 (例)

    在坦克大战裁判中, 需要修改以下参数:

    (1) judge.cpp

        参数数量     _parameter_count (在命名空间training中)

    (2) bot1.h / bot2.h

        参数分配    parameters[] (1361行)

        _self_distance_weight = parameters[0];

        _depth_weight = parameters[1];

        _self_near_base_weight = parameters[2];

        _multitank_far_punishment = parameters[3]; (例)

## 功能说明

    DEBUG   打开/关闭调试输出

    WRITE_TO_FILE   打开/关闭文件输出

    *您可以从temp.txt中获取每一代的最优个体

    read_from_file(功能函数参数) 从缓存文件中读入上一代种群

    *从chrom文件读入, 要求之前打开过文件输出

## TroubleShoot

    1.  index out of range  打开DEBUG, 检查裁判输出是否为空

    2.  numpy相关问题   检查参数数量、初始值、上下界是否匹配，若匹配则多半是我的锅