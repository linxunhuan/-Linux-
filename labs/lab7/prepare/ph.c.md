```c
// 定义哈希表的条目结构
struct entry {
  int key;                // 键
  int value;              // 值
  struct entry *next;     // 指向下一个条目的指针，用于链表存储冲突的条目
};

// 哈希表的桶数组，每个桶是一个链表的头节点
struct entry *table[NBUCKET];

// 每个桶的互斥锁，用于确保每个桶在并发环境下的安全访问
pthread_mutex_t locks[NBUCKET];

// 定义一个数组存储所有的键
int keys[NKEYS];

// 使用的线程数量
int nthread = 1;

// 获取当前时间的函数，返回秒和微秒的组合
double now()
{
  struct timeval tv;
  gettimeofday(&tv, 0);  // 获取当前的时间，存储到 tv 结构中
  return tv.tv_sec + tv.tv_usec / 1000000.0;  // 返回秒数加上微秒转换为秒后的值
}

// 插入新条目的函数，将新条目插入链表的头部
static void insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));  // 分配内存来存储一个新的条目
  e->key = key;              // 设置键
  e->value = value;          // 设置值
  e->next = n;               // 将新的条目的 `next` 指向链表的当前头部
  *p = e;                    // 更新指针，令桶的头指针指向新的条目
}

// `put` 函数用于插入或更新键值对
static void put(int key, int value)
{
  int i = key % NBUCKET;  // 通过对 NBUCKET 取模来确定键值对应的桶的索引

  // 加锁，确保对桶 `i` 的访问是线程安全的
  pthread_mutex_lock(&locks[i]);

  // 遍历桶 `i` 中的链表，查找是否已经存在相同的键
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)  // 如果找到了相同的键
      break;  // 跳出循环
  }

  if (e) {
    // 如果找到了相同的键，则更新其值
    e->value = value;
  } else {
    // 如果没有找到相同的键，则插入新的键值对
    insert(key, value, &table[i], table[i]);
  }

  // 解锁，完成对桶的操作
  pthread_mutex_unlock(&locks[i]);
}

// `get` 函数用于根据给定的键查找并返回对应的条目
static struct entry* get(int key)
{
  int i = key % NBUCKET;  // 通过对 NBUCKET 取模来确定键值对应的桶的索引

  // 遍历桶 `i` 中的链表，查找是否存在对应的键
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)  // 找到相应的键
      break;  // 跳出循环
  }

  return e;  // 返回找到的条目，若没有找到则返回 NULL
}

// 线程函数，用于执行插入操作
static void *put_thread(void *xa)
{
  int n = (int) (long) xa; // 获取当前线程的编号 (xa 被转换为 long 类型再转换为 int)
  int b = NKEYS / nthread; // 每个线程处理的键的数量

  // 每个线程遍历自己分配的键数组，并调用 put 函数将键值对插入哈希表
  for (int i = 0; i < b; i++) {
    put(keys[b * n + i], n);  // 调用 put 将键值对插入哈希表
  }

  return NULL;  // 线程执行完毕后返回 NULL
}

// 线程函数，用于执行查询操作
static void *get_thread(void *xa)
{
  int n = (int) (long) xa; // 获取当前线程的编号 (xa 被转换为 long 类型再转换为 int)
  int missing = 0;  // 用于记录查询中没有找到的键的数量

  // 每个线程遍历所有的键并进行查询
  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);  // 调用 get 函数查询键是否存在
    if (e == 0) missing++;  // 如果没有找到对应的条目，则计数
  }

  // 输出当前线程检查的缺失键的数量
  printf("%d: %d keys missing\n", n, missing);

  return NULL;  // 线程执行完毕后返回 NULL
}

int main(int argc, char *argv[])
{
  pthread_t *tha;  // 存储线程 ID 的数组
  void *value;     // 用于接收 pthread_join 时返回的值
  double t1, t0;   // 用于记录操作开始和结束时间，计算执行时间

  // 检查命令行参数是否提供了线程数
  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);  // 如果没有提供线程数，退出程序
  }
  nthread = atoi(argv[1]);  // 将命令行参数转换为线程数
  tha = malloc(sizeof(pthread_t) * nthread);  // 为线程 ID 数组分配内存
  srandom(0);  // 设置随机数种子，保证每次生成的随机数相同
  assert(NKEYS % nthread == 0);  // 确保键的总数能够被线程数整除

  // 初始化所有键
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();  // 为每个键生成一个随机数
  }

  // 初始化每个桶的互斥锁
  for (int i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&locks[i], NULL);  // 为每个桶初始化互斥锁
  }

  // 第一阶段：执行插入操作
  t0 = now();  // 记录开始时间
  // 创建并启动多个插入线程，每个线程负责一部分键值对的插入
  for (int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  // 等待所有插入线程完成
  for (int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);  // 等待每个线程的完成
  }
  t1 = now();  // 记录结束时间

  // 输出插入操作的结果：插入的键值对数量、所用时间和每秒插入的键值对数
  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  // 第二阶段：执行查询操作
  t0 = now();  // 记录开始时间
  // 创建并启动多个查询线程，每个线程负责查询一部分键
  for (int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  // 等待所有查询线程完成
  for (int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);  // 等待每个线程的完成
  }
  t1 = now();  // 记录结束时间

  // 输出查询操作的结果：查询的总键数、所用时间和每秒查询的键数
  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS * nthread, t1 - t0, (NKEYS * nthread) / (t1 - t0));

  return 0;  // 程序结束
}
```