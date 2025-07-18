## 后台查看线程在哪个CPU上运行
- 1.ps -o pid,tid,psr,comm -T -p $(pgrep aimrt)
- 2.top -H -p $(pgrep aimrt) -b -n1
