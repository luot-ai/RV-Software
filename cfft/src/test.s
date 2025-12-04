// ALU Stress Test - Maximizing ALU Utilization and IPC
// Pure computational workload with minimal branches and memory access

.text

.macro m_exit test 
        ldr r12,=\test
        sub pc, pc, #12
.endm

_Reset:
   mov sp, #4000
   mov r12, #0
   mov r7, #0          // Success indicator

   bl main
   mov r7, #0          // Success indicator

here: b here           // Infinite loop to end program
