# B+树 bulkloading 过程的理解

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
- [ ] ddl：2022 年 1 月 6 日 23:59:59



## 项目代码

### 结点数据结构

#### 基础结点

在 `b_node.h` 中定义了 B+ 树的结点基类 `BNode`，索引节点 `BIndexNode`，叶子结点 `BLeafNode`，`b_node.cc` 实现相应的函数与方法。

`BNode` 结点作为基础类，具有基础属性：

- 表示当前结点层数 `level_`
- 当前结点数据数量 `num_entries_`
- 兄弟节点的地址 `left_sibling_` 和 `right_sibling_`
- 键值数组 `int *key_`
- 是否脏写标志 `dirty_`
- 当前结点的硬盘地址 `block_`，表示节点存储到文件的区域编号
- 结点最大数据容量 `capacity_` 
- 当前结点的 B+ 树  `BTree* btree_`

除了构造函数初始化各个属性和析构函数释放空间外，`BNode` 还具有基础虚函数，用于在派生类实现：

- `init(int level, Btree *btree)` 在 B 树的一层中初始化结点。
- `init_restore(BTree *btree, int block)` 从文件中初始化一颗 B 树。
- `write_to_buffer(char *buf)` 和 `read_from_buffer(char *buf)` 用于在缓冲区读写。
- `get_entry_size()` 获得当前结点数据大小。
- `find_position_by_key(float key)` 通过键值查找数据位置。
- `get_key(int index)` 通过下标访问键值数组 `key_` 中的数据。
- `get_left_sibling()` 和 `get_right_sibling` 访问左右兄弟结点以及 `set_left_sibling(int l)` 和 `set_right_sibling(int r)` 设置左右兄弟结点的地址。
- `get_block()` 获得当前硬盘地址，` set_block(int *block)` 设置当前结点硬盘地址。
- `get_num_entries()` 获取当前结点的数据数量。
- `get_level()` 获取当前结点在 B+ 树的高度。
- `isFull()` 判断当前结点数据是否超出了容量。
- `get_header_size()` 获取 B+ 树结点的头部大小 SIZECHAR + SIZEINT * 3，即层数 CHAR ，左右兄弟节点地址以及当前结点数据数量。
- `get_key_of_node()` 返回当前结点键值数组种的第一个 `key` 

#### 索引节点

`BIndexNode` 索引节点继承于 `BNode` 基础结点类，不仅具有相同的属性与函数，新增了属性：

- `int *son` 表示孩子结点的地址数组。

同时实现了父类 `BNode` 中的虚函数：

- `init(int level, BTree *btree)`，初始化一个新结点，通过读取读取 `btree->file` 文件获得当前地址以及可分配磁盘大小 `btree_->file_->get_blocklength()`，计算出当前结点最大容量 `capacity_`，用于分配孩子结点地址数组 `son_` 以及键值数组 `key_` 的空间并初始化。
- `init_restore(Btree *btree, int block)` 从硬盘中加载 B+ 树结点，并为其分配空间。
- `read_from_buffer(char *buf)` 和 `write_to_buffer(char *buf)` 从缓存中读写 B+ 树结点。
- `get_entry_size()` 获得索引节点一个数据对 `<key_, son_>` 的大小，即 SIZEFLOAT + SIZEINT 。
- `find_position_by_key(float key)` 和 `get_key(int index)` 查询函数。
- `get_left_sibling()` 和 `get_right_sibling()` 通过 `init_restore(btree_, left_sibling)` 传入当前 B+ 树和左右兄弟结点的地址，使用 `init_restore` 从硬盘地址中加载一个结点，从而获得左右兄弟结点。
- `get_son(int index)` 通过下标获得当前索引节点的孩子结点的地址 `son_[index]` 
- `add_new_child(float key, int son)` 向当前索引结点中加入数据，并且设置脏写标记。

#### 叶子结点

`BLeafNode` 叶子节点同样继承于 `BNode` 基础结点类，由于叶子节点存储数据，其具有新的属性：

- `num_keys_` 当前叶子节点的键值对数量。
- `capacity_keys_` 最大容量。
- `int *id_` 指向数据位置的数组。

同时实现了父类 `BNode` 中的虚函数，初始化函数 `init()` 和 `init_restore()` 与索引节点大致相同，但需要额外处理 key 和 value 数组的大小，其余如从缓冲区读写与获得左右节点的函数实现都类似。但叶子节点增加了一些对 key 和 value 处理的函数：

- ` get_increment() ` 查看增加的叶子节点数量。
- `get_num_keys()` 查看 keys 数量的函数。
- `get_entry_id(int index) ` 跟据下标 index 的值查看 entry 编号 `id` 的值。
- `add_new_child(float key, int id)` 新增一个到当前叶子节点的尾部。

### B+ 树数据结构

在 `b_tree.h`，`b_tree.cc` 中定义了 B+ 树的数据结构（在 QALSH 中用于构建带索引的哈希表）`BTree` 类，具有基本的树形结构属性：

- `root_` 根节点的硬盘地址
- `BNode *root_ptr` 指向根节点的指针
- `BlockFile *file_` 指向当前 B+ 树存储的硬盘文件

对 B+树除了基本的构造和析构函数外，还具有以下函数方法：

- `init(int b_length, const char *fname)` 通过指定文件名以及其大小，初始化一棵 B+ 树，并且将根节点初始化为索引节点，初始化当前的属性 `root_` 和 `root_ptr`
- `init_restore(const char *fname)` 根据硬盘中的树文件加载一棵 B+ 树，初始化过程和 `init()` 类似

- `load_root()` 和 `delete_root()` 分别对根节点加载和删除。
- `bulkload(int n, const Result *table)` 串行地从数据集 `Result* table` 中批量地载入数据，并且构建 B+ 树。

### 文件流与磁盘交互

在 `block_file.h`，`block_file.cc` 中定义了数据结构如何与磁盘进行交互，构建的 B+ 树的每一个节点会依次存储到一个文件的不同区域中，每一个节点有一个 block 号，表示节点存储到文件的区域编号。

`BlockFile` 类具有与磁盘交互的属性：

- `FILE *fp_` 文件指针
- `char *fname` 文件名 
- `new_flag_` 标记当前文件是否为新文件
- `block_length` 磁盘文件一个 block 的长度，
- `act_block` fp位置的块数
- `num_blocks` blocks 的数量

在构造函数中，需要制定 block 的长度以及文件名用于初始化当前的 BlockFile 或打开已有的 BlockFile，同时需要具有文件读写的相关函数方法：

- `put_bytes(const char *bytes, int num)` 向当前文件写入长度为 num 的 bytes 字符串
- `get_bytes(const char *bytes, int num)` 从当前文件读取长度为 num 的字符串写入到 bytes
- `seek_block(int bnum)` 使用 `fseek` 将文件流 `fp` 偏移 `bmun` 个 block 位置，即 `(*bnum*-act_block_)*block_length_` 
- `file_new()` 检查当前文件是否是新文件
- `get_blocklength()` 获取一个 block 块的长度
- `get_num_of_blocks()` 获取所有 block 块的数量
- `fwrite_number(int num)` 使用上述 `put_bytes()` 函数向文件流中写入一个数
- `fread_number()` 使用上述 `get_bytes()` 函数从文件流中读取一个数
- `read_header(char *buffer)` 和 `set_header(char *buffer)` 从缓冲区中读取或设置剩余字节，即写入或读取当前第一个 block 块的值，但使用了 `fseek(fp_, BFHEAD_LENGTH, SEEK_SET);` 偏移，即并不是操作当前 blockfile 的 header，
- `read_block(Block block, int index)` 和 `write_block(Block block, int index)` 使用下标 index 读取 BlockFile 中第 index 个 block 并写入到参数 block 字符串中，或读取当前参数 block 块中的数据，写入到 BlockFile 中。
- `append_block(Block block)` 将一个 block 块添加到当前文件流 BlockFile 中
- `delete_last_blocks(int num)`  通过偏移删除当前文件流中最后 num 个 block 块

### 辅助文件

在 `pri_queue.h` 和 `pri_queue.cc` 中定义了基本的数据结构 `Result` 具有 `id_` 属性表示值，`key_` 表示键，以及用于比较数据的比较函数 ：`ResultComp` 升序比较函数和 `ResultCompeDesc` 降序比较函数。以及 `Mink_List` 数据结构维护最小 k 值，是近似最近邻检索算法 QALSH 中的数据结构，用于存储和查找 K 近邻，具有获取当前最大值，最小值以及第 i 个值等查询函数和插入函数，其具有 `k` 个数据，当前 `num` 个激活数据，以及 `Result *` 链表数组。

`make_data.cpp` 用于生成 B+ 树中的结点，其定义了 `Result` 结构体记录数据用于存储生成的数据，具有 `id_` 属性表示值，`key_` 表示键。通过定义结点数 `n` 与生成值的范围 `range`，使用 `random() % range` 生成随机数（`id_` 属性递增，`key_` 值随机），通过 `qsort()`  快速排序，按照 `key_` 升序排序，如果 `key_` 相同则按照 `id_` 升序排序。最后使用 `ofstream` 输出文件流，将所有节点数据按照 `key_, id_` 格式存入 `data.csv` 逗号分隔值数据文件。

在 `def.h` 中声明了 Block 字符串即磁盘中的地址。同时使用宏命令声明了一些比较函数如 MIN，MAX，以及不同类型的数值的常量如 MAXREAL，MAXINT 以及自然底数 E 和圆周率 PI 等等，但其中 INT 类型的最小值以及 REAL 实数（FLOAT）类型的最小值定义有误，参照 C/C++ numeric limits 库中的宏命令，`INT_MIN = (-INT_MAX - 1)`，而 `FLT_MIN = 1.175494e-38`。

在 `random.h`、`random.cc` 中声明了各类概率与统计学函数，用于生成随机数，如高斯分布、柯西分布、列维分布等等，以及各种分布的概率密度函数，相关系数等等统计学工具。

在 `util.h`、`util.cc` 中声明了时间变量 `g_start_time` 和 `g_start_time`，用于使用 Linux 中 `gettimeofday()` 的时间函数记录起始时间、结束时间。并且声明了统计当前运行时间、基准真相、IO/内存占用的比率等全局变量，同时定义和声明了许多实用的函数，比如 `create_dir(char *path)` 创建文件目录，`int read_txt_data(int, int, const char*, float **)` 读取数据等等。提供了各种使用的文件读写、统计时间、统计数据工具。

### 主要入口

在 `main.cc` 中定义了使用 `make_data.cpp` 随机生成数据文件 `./data/dataset.csv` ，数据的个数 `n_pts_` ，生成的 B+ 树文件流 `./result/B_tree` （用于构建 BlockFile 和存储构建 B+ 树的结果），以及 B+ 树一个结点的 block 大小 `B = 512`。

数据集 `Result *table = new Result[n_pts_] `  用于读取文件流 `dataset.csv` 中的数据，使用 `atof()` 和 `atoi` 将逗号分隔数据转换成 `float` 类型的 `key_` 值以及 `int` 类型的 `id_` 值。

读取完数据后，关闭文件流，统计起始时间 `start_t` 并且构建一棵 B+ 树并对其进行初始化，随后使用 `trees_bulkload(n_pts_, table)` 将数据集中的数据批量地载入到 B+ 树中，当构建完成后，输出串行 BulkLoad 的时间。



## 局部敏感哈希 LSH

综上，这些数据结构和辅助文件都来自于 HuangQiang 学长在 QALSH 论文中实现的源代码，其用于解决高纬度的欧几里得空间上的最近邻搜索问题，同时，这种高纬度的最近邻搜索问题可以在关系数据库中实现，比如构建索引等等。

由于在高纬中 NN 问题线性搜索非常耗时，所以需要加入索引项，以实现在常数时间内得到查找结果，比如使用 KDtree 或是近似最近邻查找（Approximate Nearest Neighbor, ANN），而 LSH (Locality-Sensitive Hashing, LSH) 就是 ANN 中的一种，在 HuangQiang 学长的源代码中也出现了 KDTree 等数据结构。

LSH 的思想与哈希产生冲突类似，如果原始数据空间中的两个相邻数据点通过哈希映射 hash function 后，产生碰撞冲突的概率较大，而不相邻的数据点经过映射后产生冲突的概率较小。就可以在相应的映射结果中继续进行线性匹配，以降低高维数据最近邻搜索的问题。LSH 用于对大量的高维数据构建索引，并且通过索引近似最近邻查找，主要应用于查找网络上的重复网页、相似网页以及图像检索等等。其中在图像检索领域中，由于其可以对图片的特征向量进行构建 LSH 索引，加快索引速度，所以在图像检索中 LSH 有着重要用途。







## 其中Bulkloading算法的具体过程如下：

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







