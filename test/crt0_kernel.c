/*
 * Minimal startup for kernel-mode tests. Provides _start so the test links with -nostdlib.
 * _start calls main(); after main returns, loops forever (run under PoOS/QEMU or use as link check).
 */
extern int main(void);

void _start(void)
{
  (void)main();
  for (;;)
    ;
}
