// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

#ifdef LAB_LOCK
// Reader-writer lock.
struct rwspinlock {
  // Replace this with your implementation.

  int readers;           // 当前持有锁的读者数量
  int writers_waiting;   // 正在等待的写者数量
  int writing;           // 是否有写者持有锁（0=没有，1=有）
  struct spinlock lock;  // 保护上面字段的锁

  //struct spinlock l;
};
#endif
