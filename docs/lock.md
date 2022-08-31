# Lock: locking classes

The lock module provides 4 lock classes that follow the <i>TimedMutex</i> requirements:
-   *seq::spinlock*: a one byte fast spinlock implementation
-   *seq::spin_mutex*: more general combination of spin locking and mutex
-   *seq::read_write_mutex*: read-write mutex based on spinlock or spin_mutex
-   *eq::null_lock*: empty lock class

See classes documentation for more details.
