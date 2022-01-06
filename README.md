# SE-DatabaseFinal

## 已测试环境：

- Ubuntu 20.04 gcc version 9.3.0
  - 编译选项：`g++ -std=c++2a -g -pthread -w`

- Ubuntu 21.10 gcc version 11.2.0
  - 编译选项：`g++ -std=c++20 -g -w`



所有代码处于当前根目录下的 `./src`，编译命令参照 `./src/Makefile`，在根目录运行，构建的 B+ 树数据在 `./data/dataset.csv`，结果在当前根目录下 `./result/B_tree`。

## 运行说明：

```shell
cd src
make clean
make
cd ../
./run
```

其中 `./run` 是并行构建 B+ 树 Bulk Load 的可执行程序，./original_run 或 `./src/run` 是原本的串行构建 B+ 树 Bulk Load 的可执行程序。



## 运行结果：

![image-20220106151020805](https://gitee.com/AdBean/img/raw/master/images/202201061510535.png)