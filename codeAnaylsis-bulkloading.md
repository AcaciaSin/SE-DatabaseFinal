# B+树bulkloading过程的理解

- [x] 作业代码。
- [ ] Readme文件，说明具体怎么运行代码，说明参数设置。
- [ ] 课程设计报告。
- [x] B+树bulkloading过程的理解（用自己语言写）
- [ ] 算法并行的设计思路
- [ ] 算法流程图
- [ ] 关键代码描述
- [ ] 实验结果
- [ ] 实验分析
- [ ] 性能调优、创新优化
- [ ] 实验心得。（小组每个同学写一份，合在一个文档中）
- [ ] 学者网提交作业。
- [ ] ddl：2022年1月6日



### 代码分析：

`main.cc`文件：

- 26-34：第26行定义了字符数组`data_file `用于存储`dataset.csv`的文件路径，第27行第一了字符数组`tree_file`用于存储`Btree`的文件路径。随后打印出来

- 36：`Result *table = new Result[n_pts_]; ` 定义了一个`Result`类型的指针`table`，其大小为`n_pts_`的值1000000，其中Result类型的介绍如下：

  ```c++
  struct Result {						// basic data structure 
  	float key_;
  	int   id_;
  };
  ```

  Result是一个包含key和id的结构体。Result比较时的规则如下，如果假设有Result定义的re1和re2，比较re1和re2的key，如果key相等则比较re1和re2的id。

- 38-53：定义了一个ifstream类型的文件流`fp`，用于打开`data_file`。使用ifstream打开txt后，需要使用getline函数读取每行的数据。然后通过`atof`函数和`atoi`函数传入前面定义的`table`中存起来。然后文件流。

- 55-58，67-71：用于计算运行时间并打印运行时间。

- 60-61：新建BTree，并进行初始化。

- 63：bulkloading操作。

- 65：销毁运行内存。



`b_node.h`、`b_node.cc`文件：

- `b_node.h`文件内共有三个类，基本节点`BNode`，继承BNode的节点`BIndexNode`,继承BNode的节点`BLeafNode`。BNode的成员如下：

```c++
protected:
	char  level_;					// level of b-tree (level > 0)
	int   num_entries_;				// number of entries in this node
	int   left_sibling_;			// addr in disk for left  sibling
	int   right_sibling_;			// addr in disk for right sibling
	float *key_;					// keys

	bool  dirty_;					// if dirty, write back to file
	int   block_;					// addr of disk for this node
	int   capacity_;				// max num of entries can be stored
	BTree *btree_;					// b-tree of this node
```

`BIndexNode`增加了成员(树的索引节点)：

```c++
protected:
	int *son_;						// addr of son node
```

`BLeafNode`增加了成员（树的叶子结点）：

```c++
protected:
	int num_keys_;					// number of keys
	int *id_;						// object id
	int capacity_keys_;				// max num of keys can be stored
```

- `BNode`类型的节点，能做的操作有：
  - 读/写buffer
  - 获得`entry`索引项的大小
  - 根据`key`的值查找位置
  - 根据`index`的值查找`key`
  - 返回左右节点。
  - 获取Block
  - 获取entries的数目
  - 返回层数level
  - 返回header的大小
  - 返回节点的key值
  - 判断索引项entry是否已经满了。
- `BIndexNode`类型的节点，继承了`BNode`类型的节点，增加了
    - ` get_son(int index)`获得孩子的index值函数
    - `add_new_child(float key, int son)`根据给定的编号`id`和key值新增孩子节点的函数。
- `BLeafNode`类型的节点，继承了`BNode`类型的节点，增加了：
  - ` get_increment()`函数，查看增加的叶子节点数量；
  - `get_num_keys()`查看keys数量的函数；
  - `get_entry_id(int index) `跟据index的值查看entry编号`id`的函数；
  - `add_new_child(float key, int son)`根据给定的编号`id`和key值新增孩子节点的函数。





`block_file.h`，`block_file.cc`文件：

- `BlockFile`是b-tree读写文件的结构。`BlockFile`包括了文件指针`fp`，文件名`fname`，`new_flag_`，块的长度，fp位置的块数，总的块数

```c++
public:
	FILE *fp_;						// file pointer
	char fname_[200];				// file name
	bool new_flag_;					// specifies if this is a new file
	
	int block_length_;				// length of a block
	int act_block_;					// block num of fp position
	int num_blocks_;				// total num of blocks
```

- `BlockFile`类能进行的操作有：
  - `put_bytes(const char *bytes, int num)` 写num长度的bytes
  - `get_bytes(const char *bytes, int num)` 读num长度的bytes
  - `seek_block(int bnum)`将`fp`向右移动`bmun`的值
  - `file_new()`查看`block`是否被更改过
  - `get_blocklength()`获得`block`的长度
  - `get_num_of_blocks()`获得`block`的数量
  - `fwrite_number(int num)`写值
  - `fread_number()`读值
  - `read_header`读取剩余字节，不包括头
  - `set_header`设置剩余字节，不包括头
  - `read_block`用块号读块
  - `write_block`用块号写块
  - `append_block`把一个`block`加在文件的后面
  - `delete_last_blocks`删掉最后的num个`block`



`b_tree.h`，`b_tree.cc`文件：

- `BTree`是对qalsh生成的哈希表进行索引的一棵树，其数据成员有：根节点`root`，指向根节点的指针，以及存储在硬盘里面的`BlockFile`类型的文件`file_`

```c++
public:
	int root_;						// address of disk for root
	BNode *root_ptr_;				// pointer of root
	BlockFile *file_;				// file in disk to store
```

- 可以对`BTree`进行的操作有：
  - 初始化BTree：`init(int b_length, const char *fname)`
  - 根据文件名加载B-tree：`init_restore(const char *fname)`
  - bulkloading操作：`bulkload(int n, const Result *table)`
  - 从缓冲区读根节点：`read_header(const char *buf)`
  - 把根节点写入缓冲区内：`writer_header(const char *buf)`
  - 加载根节点：`load_root()`
  - 删除根节点：`delete_root()`

##### 其中Bulkloading算法的具体过程如下：

`int BTree:bulkload(int n, const Result *table)`函数：

- 主要功能：从内存里批量构建一棵树，内存里面的数据是有序的数据

- 第一步：定义需要用到的节点

  - index_child：索引节点的子节点
  - index_prev_nd：索引节点的前n个节点
  - index_act_nd：？
  - leaf_child：叶子节点
  - leaf_prev_nd：叶子节点的前n个节点
  - leaf_act_nd ：
  - id编号：初始化为-1
  - block块号：初始化为-1
  - key：初始化为最小实数

- 第二步：使用哈希表批量构建叶子节点

  - 首先把`first_node`的值初始化为`true`，第一个节点存储的开始block和结束block的编号初始化为0。

- 第三步：进入for循环。for循环的次数是entry的数目。

  - 首先把table（从data.csv加载的）的每一个键值对赋值给变量`id`和`key`。

  - 如果`leaf_act_nd` 为空，则新构建一个`leaf_act_nd`，并初始化，定义层数为0，给予一个硬盘的空间。

    然后判断`first_node`是否为true：如果为true则把`first_node`改为false，表明已经初始化好开始的`block`；否则，把新构建的叶子结点`leaf_act_nd`的左兄弟节点设置为`leaf_prev_nd`，`leaf_prev_nd`的右兄弟节点设置为`leaf_act_nd`。即把他们用双向链表的方式链接起来。随后删除`leaf_prev_nd`的指针，重新设置`leaf_prev_nd`为空指针。

    把结束的区块`end_block`设置为`leaf_act_nd`的当前区块。

  -  把table加载的一个键值对增为`leaf_act_nd`的儿子节点，即新增索引项。

  - 如果`leaf_act_nd`的索引项已满，则把`leaf_act_nd`的前一个节点`leaf_prev_nd`指向`leaf_act_nd`，让`leaf_act_nd`为空指针。即更改当前节点，以继续新增entry。

  - 退出for循环，完成所有底层叶子结点的构建。

- 第四步：把`leaf_prev_nd`指针和`leaf_act_nd`指针空间释放并设置为空指针。

- 第五步：为自底向上构建索引节点做准备。初始化`current_level`，即当前的层数，且我们认为叶子结点所在的层数为0。定义`last_start_block` 最后一个开始的块为内存中最开始块的块号`start_block`，`last_end_block`最后一个结束的块为内存中结束块的块号 `end_block`。

- 第六步：进入while循环，while循环的结束条件为：最后一个结束的块与最后一个开始块的块号相同。

  - 首先把`first_node`的值初始化为`true`。
  - 进入for循环，for循环的次数为`last_end_block`块号- `last_start_block`块号的值。
    - 让变量`block`的值为循环体`i`的值
    - 如果当前层是叶子结点的上一层，即层数为1，如果是则：新构建一个`leaf_child`，并根据`block`的块号从内存中导入进来，并让其key值为`leaf_child`的key值。然后把`leaf_child`重新设置为空指针；否则，新构建一个`index_child`，并根据`block`的块号从内存中导入进来，并让其key值为`index_child`的key值。然后把`index_child`重新设置为空指针。
    - 判断`index_act_nd`是否为空指针：如果不为空指针则：新构建一个`index_act_nd`，并根据`block`的块号从内存中导入进来。然后判断`first_node`的值：
      - 如果为真，则改变其值为false，并修改`start_block`，让其值为`index_act_nd`的block的值
      - 否则，把新构建的叶子结点`index_act_nd`的左兄弟节点设置为`index_prev_nd`，`index_prev_nd`的右兄弟节点设置为`index_act_nd`。即把他们用双向链表的方式链接起来。随后删除`index_prev_nd`的指针，重新设置`index_prev_nd`为空指针。
      - 修改`end_block`，让其值为`index_act_nd`的block的值。
    - 然后就可以往`index_act_nd`新增entry，该entry的`key`和`block`值索引节点的儿子节点的`key`和`block`值。
    - 如果`index_act_nd`的索引项已满，则把`index_act_nd`的前一个节点`index_prev_nd`指向`index_act_nd`，让`leaf_act_nd`的值为空指针。即更改当前节点，以继续新增entry。
    - 退出for循环。
  - 把`index_prev_nd`指针和`index_act_nd`指针空间释放并设置为空指针。
  - 更新`last_start_block`的值为新的`start_block`的值，` last_end_block` 为新的 `end_block`的值; 并增加层数。即继续往上层构建。直至退出while循环，此时构建完所有的索引节点。

- 第七步：把`root_`根节点在硬盘中的地址修改为内存中最后一块block的起始编号。

- 第八步：释放所有指针的内存空间。





`pri_queue.h`、`pri_queue.cc`文件：

- 定义了`Result`的结构体、`Mink_List`的结构体。其中Result时结果，其上的操作有：比较大小、插入，计算最大`key`值、最小`key`值等。



`make_data.cpp` 文件：

定义 Result 结构体记录数据

- 并随机生成值，qsort 排序后，存入`data.csv`表格中。



`def.h` 文件：

在 `def` 中使用宏命令声明了一些比较函数如 MIN，MAX，以及不同类型的数值的常量如 MAXREAL，MAXINT 以及自然底数 E 和圆周率 PI 等等，但其中 INT 类型的最小值以及 REAL 实数（FLOAT）类型的最小值定义有误，参照 C/C++ numeric limits 库中的宏命令，`INT_MIN = (-INT_MAX - 1)`，而 `FLT_MIN = 1.175494e-38`



`random.h`、`random.cc` 文件：

在 `random` 中声明了各类概率与统计学函数，用于生成随机数，如高斯分布、柯西分布、列维分布等等，以及各种分布的概率密度函数，相关系数等等统计学工具。



`util.h`、`util.cc` 文件：

在 `util` 中声明了时间变量 `g_start_time` 和 `g_start_time`，用于使用 Linux 中 `gettimeofday` 的时间函数记录起始时间、结束时间。

并且声明了统计当前运行时间、基准真相、IO/内存占用的比率等全局变量，同时定义和声明了许多实用的函数，比如 `create_dir(char *path)` 创建文件目录，`int read_txt_data(int, int, const char*, float **)` 读取数据等等。

综上，在 `util.h`、`util.cc` 文件中提供了各种使用的文件读写、统计时间、统计数据工具。



