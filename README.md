# 有一万个Bug来不及改的 Tank2S AI

by Mt_Nomad 12f23eddde

## 文件结构
judgesrc/   支持自我对局参数优化的Tank2S裁判

geasrc/     遗传算法调参脚本+说明文档

tank2s.cpp  极为早期的版本(特判为主)

tank2s_stable.cpp   较为稳定的版本(没有搜索)

Tank2S_lt.cpp   应用Minimax搜索的版本

Tank2S_iterate.cpp  迭代收敛估值的版本(可能有Bug)

tank2s_final.cpp    参加决赛的版本
