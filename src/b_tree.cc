#include "b_tree.h"

#include <chrono>
#include <memory>
#include <queue>
#include <thread>
#include <tuple>

// -----------------------------------------------------------------------------
//  BTree: b-tree to index hash values produced by qalsh
// -----------------------------------------------------------------------------
BTree::BTree()						// default constructor
{
	root_     = -1;
	file_     = NULL;
	root_ptr_ = NULL;
}

// -----------------------------------------------------------------------------
BTree::~BTree()						// destructor
{
	char *header = new char[file_->get_blocklength()];
	write_header(header);			// write <root_> to <header>
	file_->set_header(header);		// write back to disk
	delete[] header; header = NULL;

	if (root_ptr_ != NULL) {
		delete root_ptr_; root_ptr_ = NULL;
	}
	if (file_ != NULL) {
		delete file_; file_ = NULL;
	}
}

// -----------------------------------------------------------------------------
void BTree::init(					// init a new tree
	int   b_length,						// block length
	const char *fname)					// file name
{
	FILE *fp = fopen(fname, "r");
	if (fp) {						// check whether the file exist
		fclose(fp);					// ask whether replace?
		// printf("The file \"%s\" exists. Replace? (y/n)", fname);

		// char c = getchar();			// input 'Y' or 'y' or others
		// getchar();					// input <ENTER>
		// assert(c == 'y' || c == 'Y');
		remove(fname);				// otherwise, remove existing file
	}			
	file_ = new BlockFile(b_length, fname); // b-tree stores here

	// -------------------------------------------------------------------------
	//  init the first node: to store <blocklength> (page size of a node),
	//  <number> (number of nodes including both index node and leaf node), 
	//  and <root> (address of root node)
	// -------------------------------------------------------------------------
	root_ptr_ = new BIndexNode();
	root_ptr_->init(0, this);
	//返回BIndexNode中的变量block_
	root_ = root_ptr_->get_block();
	//释放root-ptr的内存
	delete_root();
}

// -----------------------------------------------------------------------------
void BTree::init_restore(			// load the tree from a tree file
	const char *fname)					// file name
{
	FILE *fp = fopen(fname, "r");	// check whether the file exists
	if (!fp) {
		printf("tree file %s does not exist\n", fname);
		exit(1);
	}
	fclose(fp);

	// -------------------------------------------------------------------------
	//  it doesn't matter to initialize blocklength to 0.
	//  after reading file, <blocklength> will be reinitialized by file.
	// -------------------------------------------------------------------------
	file_ = new BlockFile(0, fname);
	root_ptr_ = NULL;

	// -------------------------------------------------------------------------
	//  read the content after first 8 bytes of first block into <header>
	// -------------------------------------------------------------------------
	char *header = new char[file_->get_blocklength()];
	file_->read_header(header);		// read remain bytes from header
	read_header(header);			// init <root> from <header>

	delete[] header; header = NULL;
}

// -----------------------------------------------------------------------------
int BTree::bulkload(      // bulkload a tree from memory
    int n,                // number of entries
    const Result *table)  // hash table
{
  int start_block = 1;     // position of first node, always be 1
  int end_block = 0;       // position of last node

  const auto workerThreadsCount = std::thread::hardware_concurrency() - 1;
  assert(workerThreadsCount >= 1);
  std::vector<std::thread> workerThreads;

  const auto headerSize = SIZECHAR + SIZEINT * 3;
  const auto keySize =
      ((int)ceil((float)file_->get_blocklength() / LEAF_NODE_SIZE) * SIZEFLOAT +
       SIZEINT);
  const auto entrySize = SIZEINT;
  const auto treeNodesCapacity =
      (file_->get_blocklength() - headerSize - keySize) / entrySize;
  const auto leafNodesCount = (int)ceil((double)n / treeNodesCapacity);
  const auto bound = leafNodesCount / workerThreadsCount;
  auto lock = std::make_unique<SpinLock>();
  auto tree = this;

  using Tuple = std::tuple<int, BLeafNode *>;
  const auto compare = [](const Tuple &a, const Tuple &b) {
    return std::get<0>(a) > std::get<0>(b);
  };
  std::priority_queue<Tuple, std::vector<Tuple>, decltype(compare)> heap(
      compare);

  auto consumerThread =
      std::thread([&lock, &heap, tree, &end_block, &leafNodesCount, n] {
        int processedNodes = 0;
        int lastLeafIndex = -1;
        int lastBlockIndex = 0;  // 这里的情况和 start_block = 1 的原因一样，file_ 里第一个 block 一定是树根节点

        while (processedNodes < leafNodesCount) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));

          lock->lock();
          const auto heapSize = heap.size();
          char *data = new char[heapSize * tree->file_->get_blocklength()];
          bool isFirst = true;
          int entryIndex = 0;
          BLeafNode *prev;
          while (!heap.empty()) {
            auto [leafIndex, leafNode] = heap.top();
            if (leafIndex != lastLeafIndex + 1) {
              break;
            }
            lastLeafIndex = leafIndex;
            processedNodes++;

            if (isFirst) {
              isFirst = false;
              if (lastBlockIndex > 0) {
                leafNode->set_left_sibling(lastBlockIndex);
              }
              leafNode->set_block(lastBlockIndex + entryIndex + 1);
              prev = leafNode;
            } else {
              leafNode->set_block(lastBlockIndex + entryIndex + 1);
              leafNode->set_left_sibling(prev->get_block());
              prev->set_right_sibling(leafNode->get_block());
              prev->write_to_buffer(data + (entryIndex - 1) *
                                               tree->file_->get_blocklength());
              delete prev;
              prev = leafNode;
            }

            entryIndex++;
            heap.pop();
          }
          lock->unlock();

          if (!entryIndex) {
            continue;
          }

          prev->set_block(lastBlockIndex + entryIndex);
          end_block = prev->get_block();
          if (end_block > n) {
            throw;
          }
          if (processedNodes < leafNodesCount) {
            prev->set_right_sibling(lastBlockIndex + entryIndex + 1);
          }
          prev->write_to_buffer(data + (entryIndex - 1) *
                                           tree->file_->get_blocklength());
          delete prev;
          prev = nullptr;

          // 这里写入的大小不是 heapSize 而是 entryIndex
          lastBlockIndex =
              tree->file_->write_blocks(data, entryIndex, lastBlockIndex);
          delete[] data;
          data = nullptr;
        }
      });

  for (int i = 0; i < workerThreadsCount; i++) {
    workerThreads.emplace_back(std::thread(
        [=, &lock, &heap](int id) {
          for (int h = 0; h <= bound; h++) {
            auto leafIndex = id + h * workerThreadsCount;
            if (leafIndex >= leafNodesCount) {
              continue;
            }
            auto leafNode = new BLeafNode();
            leafNode->init(0, tree);
            for (int k = 0; k < treeNodesCapacity; k++) {
              auto index = leafIndex * treeNodesCapacity + k;
              if (index >= n) {
                break;
              }
              auto id = table[index].id_;
              auto key = table[index].key_;
              leafNode->add_new_child(id, key);
              if (k < treeNodesCapacity - 1) {
                assert(!leafNode->isFull());
              }
            }
            lock->lock();
            heap.emplace(std::make_tuple(leafIndex, leafNode));
            lock->unlock();
          }
        },
        i));
  }

  consumerThread.join();
  for (auto &thread : workerThreads) {
    thread.join();
  }

  workerThreads.clear();

  load_index_layers(start_block, end_block);

  return 0;
}

void BTree::load_index_layers(int start_block, int end_block) {
  int current_level = 1;               // current level (leaf level is 0)
  int last_start_block = start_block;  // build b-tree level by level
  int last_end_block = end_block;      // build b-tree level by level

  // const auto workerThreadsCount = std::thread::hardware_concurrency() - 1;
  const auto workerThreadsCount = 1;
  assert(workerThreadsCount >= 1);
  const auto headerSize = SIZECHAR + SIZEINT * 3;
  const auto entrySize = SIZEFLOAT + SIZEINT;
  // 每一层需要扫描的 block 总数
  const auto totalBlocksCount = last_end_block - last_start_block;
  // 一个 index node 的容量
  const auto nodeCapacity = (file_->get_blocklength() - headerSize) / entrySize;
  // 每一层要构建的 index node 总数
  const auto todoNodesCount =
      (int)ceil((double)totalBlocksCount / nodeCapacity);

  while (last_end_block > last_start_block) {
    auto lock = std::make_unique<SpinLock>();
    auto tree = this;
    auto currentData = std::make_tuple<int, char *>(-1, nullptr);

    using Tuple = std::tuple<int, BIndexNode *>;
    const auto compare = [](const Tuple &a, const Tuple &b) {
      return std::get<0>(a) > std::get<0>(b);
    };
    std::priority_queue<Tuple, std::vector<Tuple>, decltype(compare)> heap(
        compare);

    std::vector<std::thread> workerThreads;

    auto composerThread = std::thread([=, &lock, &heap, &todoNodesCount,
                                       &start_block, &end_block, &currentData] {
      int loadedBlocksCount = 0;
      int processedNodesCount = 0;
      int lastIndex = -1;
      int lastBlockIndex = last_end_block;

      while (processedNodesCount < todoNodesCount) {
        const auto loadedCount =
            std::min(nodeCapacity * 1000, totalBlocksCount - loadedBlocksCount);
        loadedBlocksCount += loadedCount;
        char *data = new char[tree->file_->get_blocklength() * loadedCount];
        assert(tree->file_->read_blocks(data,
                                        last_start_block + processedNodesCount,
                                        loadedCount) == true);
        lock->lock();
        currentData = {loadedCount, data};
        lock->unlock();

        auto processedCountOfCurrentRun = 0;
        const auto indexNodesCountOfCurrentRun =
            (int)ceil((double)loadedCount / nodeCapacity);
        while (processedCountOfCurrentRun < indexNodesCountOfCurrentRun) {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));

          lock->lock();
          const auto heapSize = heap.size();
          char *data = new char[heapSize * tree->file_->get_blocklength()];
          bool isFirst = true;
          int entryIndex = 0;
          BIndexNode *prev;
          while (!heap.empty()) {
            auto [indexNodeIndex, indexNode] = heap.top();
            if (indexNodeIndex != lastIndex + 1) {
              break;
            }
            lastIndex = indexNodeIndex;
            processedCountOfCurrentRun++;

            if (isFirst) {
              isFirst = false;
              indexNode->set_block(lastBlockIndex + entryIndex + 1);
              if (lastBlockIndex > last_end_block) {
                indexNode->set_left_sibling(
                    lastBlockIndex);  // 不是第一个 block，直接和上一个
                                      // block 相连
              } else {
                start_block =
                    indexNode->get_block();  // 这是这个索引层的第一个 block
              }
              prev = indexNode;
            } else {
              indexNode->set_block(lastBlockIndex + entryIndex + 1);
              indexNode->set_left_sibling(prev->get_block());
              prev->set_right_sibling(indexNode->get_block());
              prev->write_to_buffer(data + (entryIndex - 1) *
                                               tree->file_->get_blocklength());
              delete prev;
              prev = indexNode;
            }

            entryIndex++;
            heap.pop();
          }
          lock->unlock();

          if (!entryIndex) {
            continue;
          }

          prev->set_block(lastBlockIndex + entryIndex);
          end_block = prev->get_block();  // 更新 end_block 指针
          if (processedNodesCount + processedCountOfCurrentRun <
              todoNodesCount) {
            prev->set_right_sibling(lastBlockIndex + entryIndex + 1);
          }
          prev->write_to_buffer(data + (entryIndex - 1) *
                                           tree->file_->get_blocklength());
          delete prev;
          prev = nullptr;

          // 这里写入的大小不是 heapSize 而是 entryIndex
          lastBlockIndex =
              tree->file_->write_blocks(data, entryIndex, lastBlockIndex);
          delete[] data;
          data = nullptr;
        }

        // 这一轮读取结束
        processedNodesCount += processedCountOfCurrentRun;
      }

      lock->lock();
      currentData = std::make_tuple<int, char *>(-1, nullptr);
      lock->unlock();
    });

    for (int i = 0; i < workerThreadsCount; i++) {
      workerThreads.emplace_back(std::thread(
          [=, &currentData, &lock, &heap, &current_level, &end_block](int id) {
            int blocksCountOfCurrentRun = -1;
            char *data = nullptr;
            while (true) {
              while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                lock->lock();
                if (std::get<1>(currentData) != data) {
                  if (data != nullptr && std::get<1>(currentData) == nullptr) {
                    lock->unlock();
                    return;
                  }
                  blocksCountOfCurrentRun = std::get<0>(currentData);
                  data = std::get<1>(currentData);
                  lock->unlock();
                  break;
                }
                lock->unlock();
              }

              const auto indexNodesCountOfCurrentRun =
                  (int)ceil((double)blocksCountOfCurrentRun / nodeCapacity);
              const auto bound =
                  indexNodesCountOfCurrentRun / workerThreadsCount;
              for (int h = 0; h <= bound; h++) {
                // 这个 index 是 composerThread 给的这一波 totalCount 个 block
                // 数据里的相对顺序 接下来这个 workerThread
                // 会算出自己应该处理哪一部分的block，构建出对应的 index node
                auto indexNodeIndex = id + h * workerThreadsCount;
                if (indexNodeIndex >= indexNodesCountOfCurrentRun) {
                  continue;
                }
                auto indexNode = new BIndexNode();
                indexNode->init_no_write(current_level, tree);
                for (int k = 0; k < nodeCapacity; k++) {
                  auto index = indexNodeIndex * nodeCapacity + k;
                  if (index >= blocksCountOfCurrentRun) {
                    break;
                  }
                  float key;
                  auto block =
                      start_block + index;  // 此时就已经可以知道 block 号了
                  if (current_level == 1) {
                    BLeafNode node;
                    node.init_restore_in_place(
                        tree, block,
                        data + tree->file_->get_blocklength() * index);
                    key = node.get_key_of_node();
                  } else {
                    BIndexNode node;
                    node.init_restore_in_place(
                        tree, block,
                        data + tree->file_->get_blocklength() * index);
                    key = node.get_key_of_node();
                  }
                  indexNode->add_new_child_no_dirty(key, block);
                  if (k < nodeCapacity - 1) {
                    assert(!indexNode->isFull());
                  }
                }
                lock->lock();
                heap.emplace(std::make_tuple(indexNodeIndex, indexNode));
                lock->unlock();
              }
            }
          },
          i));
    }

    composerThread.join();
    for (auto &thread : workerThreads) {
      thread.join();
    }

    workerThreads.clear();

    last_start_block = start_block;  // update info
    last_end_block = end_block;      // build b-tree of higher level
    ++current_level;
  }
  root_ = last_start_block;  // update the <root>
}

// -----------------------------------------------------------------------------
void BTree::load_root() 		// load root of b-tree
{	
	if (root_ptr_ == NULL) {
		root_ptr_ = new BIndexNode();
		root_ptr_->init_restore(this, root_);
	}
}

// -----------------------------------------------------------------------------
void BTree::delete_root()		// delete root of b-tree
{
	if (root_ptr_ != NULL) { delete root_ptr_; root_ptr_ = NULL; }
}
