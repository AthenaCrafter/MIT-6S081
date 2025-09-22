# MIT 6.S081 - Operating System Engineering Lab Assignments

This repository contains my solutions for the MIT 6.S081 (Fall 2020) course lab assignments. Each lab focuses on different aspects of operating system development in the xv6 educational operating system.

## Lab Branches

The repository is organized into branches corresponding to each lab assignment:

1. **syscall** - Implementing system calls in xv6
2. **pgtbl** - Exploring page tables and virtual memory
3. **trap** - Handling traps and interrupts
4. **cow** - Implementing copy-on-write fork
5. **thread** - User-level threading implementation
6. **net** - Network driver and stack implementation
7. **file system** - xv6 file system modifications
8. **mmap** - Memory-mapped files implementation

## Getting Started

To work with these labs:

1. Clone the repository:
   ```bash
   git clone https://github.com/AthenaCrafter/MIT-6S081.git
   ```

2. Checkout a specific lab branch:
   ```bash
   git checkout [lab-name]
   ```

3. Build and run xv6:
   ```bash
   make qemu
   ```

## Course Information

MIT 6.S081 is an introductory course to operating systems that:
- Focuses on the design and implementation of operating systems
- Uses xv6, a simple Unix-like teaching operating system
- Covers virtual memory, file systems, threads, context switches, kernels, interrupts, system calls, interprocess communication, and more

Original course materials can be found on the https://pdos.csail.mit.edu/6.S081/2021/.
