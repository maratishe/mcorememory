
This project is for packet traffic capture on multicore architectures.  The target is unusual since normally traffic -- even high-rate traffic today is captured on one core.  There is a reason for it -- it is hard to partition input across multiple cores.  More complexity is added by the now parallel process where cores have to communicate with each other via message-passing or memory locking. 

This code solves exactly these problems.  Specifically:

 1. Traffic is split based on IP prefix. 
 2. A new shared memory design is used. C/C++ Pointers can be stored in shared memory, making it possible for the manager process to access DLL or processes running on cores. 
 3. The process is designed to be lock-free, meaning that neither message passing nor memory locking is used.  The process is therefore streamlined to the limit of whatever the hardware can offer. 


See `diagrams.pdf` for visuals explaining the design. 

Work in progress. Will be upgraded as interest to the topic emerges.
 

> Written with [StackEdit](https://stackedit.io/).