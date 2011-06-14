#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.h"

struct rcu {
  void *item;
  unsigned long epoch;
  struct rcu *next;
  void (*dofree)(void *);
};
static struct { struct rcu *x __attribute__((aligned (CACHELINE))); } rcu_delayed_head[NCPU];
static struct { struct rcu *x __attribute__((aligned (CACHELINE))); } rcu_delayed_tail[NCPU];
static uint global_epoch __attribute__ ((aligned (CACHELINE)));
static struct { uint x __attribute__((aligned (CACHELINE))); } min_epoch[NCPU];
static struct { struct spinlock l __attribute__((aligned (CACHELINE))); } rcu_lock[NCPU];
static struct { int v __attribute__((aligned (CACHELINE))); } delayed_nfree[NCPU];

enum { rcu_debug = 0 };

void
rcuinit(void)
{
  for (int i = 0; i < NCPU; i++)
    initlock(&rcu_lock[i].l, "rcu");
}

struct rcu *
rcu_alloc()
{
  return kmalloc(sizeof(struct rcu));
}

void *
rcu_min(uint key, void *v){
  struct proc *p = (struct proc *) v;
  if (min_epoch[cpu->id].x > p->epoch) {
      min_epoch[cpu->id].x = p->epoch;
  }
  return 0;
}

// XXX use atomic instruction to update list (instead of holding lock)
// lists of lists?
void
rcu_gc(void)
{
  struct rcu *r, *nr;
  int n = 0;
  min_epoch[cpu->id].x = global_epoch;

  ns_enumerate(nspid, rcu_min);

  pushcli();
  acquire(&rcu_lock[cpu->id].l);

  for (r = rcu_delayed_head[cpu->id].x; r != NULL; r = nr) {
    if (r->epoch >= min_epoch[cpu->id].x)
      break;
    // cprintf("free: %d (%x %x)\n", r->epoch, r->dofree, r->item);
    if (r->dofree == 0)
      panic("rcu_gc");
    r->dofree(r->item);
    delayed_nfree[cpu->id].v--;
    n++;
    rcu_delayed_head[cpu->id].x = r->next;
    if (rcu_delayed_head[cpu->id].x == 0)
      rcu_delayed_tail[cpu->id].x = 0;
    nr = r->next;
    kmfree(r);
  }
  release(&rcu_lock[cpu->id].l);
  // cprintf("rcu_gc: cpu %d n %d delayed_nfree=%d min_epoch=%d\n",
  //         cpu->id, n, delayed_nfree[cpu->id], min_epoch[cpu->id]);
  popcli();

  // global_epoch can be bumped anywhere; this seems as good a place as any
  __sync_fetch_and_add(&global_epoch, 1);
}

// XXX Use atomic instruction to update list (instead of holding lock)
void
rcu_delayed(void *e, void (*dofree)(void *))
{
  if (rcu_debug) {
    cprintf("rcu_delayed: %x %x\n", dofree, e);
    for (struct rcu *r = rcu_delayed_head[cpu->id].x; r; r = r->next)
      if (r->item == e && r->dofree == dofree)
	panic("rcu_delayed double free");
  }

  struct rcu *r = rcu_alloc();
  if (r == 0)
    panic("rcu_delayed");
  r->dofree = dofree;
  r->item = e;
  r->next = 0;
  r->epoch = global_epoch;
  pushcli();
  acquire(&rcu_lock[cpu->id].l);
  // cprintf("rcu_delayed: %d\n", global_epoch);
  if (rcu_delayed_tail[cpu->id].x != 0) 
    rcu_delayed_tail[cpu->id].x->next = r;
  rcu_delayed_tail[cpu->id].x = r;
  if (rcu_delayed_head[cpu->id].x == 0)
    rcu_delayed_head[cpu->id].x = r;
  release(&rcu_lock[cpu->id].l);
  delayed_nfree[cpu->id].v++;
  popcli();
}

void
rcu_begin_read(void)
{
  if (proc && proc->rcu_read_depth++ == 0)
    proc->epoch = global_epoch;
  __sync_synchronize();
}

void
rcu_end_read(void)
{
  if (proc && proc->rcu_read_depth > 0 && --proc->rcu_read_depth == 0)
    proc->epoch = INF;
}

void
rcu_begin_write(struct spinlock *l)
{
  if (l) acquire(l);
  __sync_synchronize();

  rcu_begin_read();
}

void
rcu_end_write(struct spinlock *l)
{
  rcu_end_read();

  if (l) release(l);
}
