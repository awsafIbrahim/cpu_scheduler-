# CPU Scheduling & Message-Passing Simulator (C)

This project implements a **multi-threaded CPU scheduling simulator** with support for **inter-process message passing** and **barrier synchronization**. It models a simplified multi-node operating system where each node has its own clock, processes, and queues. The system enforces synchronized time progression and allows processes to communicate with each other using synchronous send/receive primitives.

---

## Features

- **Scheduling**
  - Simulated multi-node environment, each with its own ready and blocked queues.
  - Round-robin style scheduling with context switching.
  - Accurate process state transitions: *new*, *ready*, *running*, *blocked*, *finished*.
- **Clock Synchronization**
  - All nodes increment their simulation clocks in lockstep using a custom barrier implementation.
- **Message Passing**
  - `SEND` and `RECV` primitives for synchronous process communication.
  - Blocking semantics: a process attempting to send or receive will block until its counterpart is ready.
- **Extensible Design**
  - Modular implementation (`barrier.c`, `message.c`, `process.c`, etc.).
  - Priority queue abstraction for managing ready/blocked lists.

---

## Running the simulator
./prosim < input.txt

Input Format

Each input describes a set of processes for a node-based system. Processes are identified by a node ID and a process ID.

Process Addressing

Each process has an address:

Address = (Node ID * 100) + (Process ID)


For example:

314 → Node 3, Process 14

202 → Node 2, Process 2


Supported Primitives

DOOP N
Perform an operation for N clock ticks.

SEND Address
Send a message to the specified destination process.
The sender blocks until the receiver is ready.

RECV Address
Receive a message from the specified source process.
The receiver blocks until the sender is ready.

HALT
Terminate the process.


Processes are grouped under declarations like:

Proc1 <arrival_time> <priority> <node_id>
<SEND / RECV / DOOP / HALT primitives>


Output Format

The simulator logs both the event trace and a summary.

Event Trace

Each scheduling decision and state transition is timestamped:

[01] 00000: process 1 new
[01] 00000: process 1 ready
[01] 00000: process 1 running
[01] 00001: process 1 blocked (send)


Where:

[NodeID] – the node reporting the event

Timestamp – global synchronized clock tick

Action – process state change

Blocked states may include:

blocked (send) – waiting on a receiver

blocked (recv) – waiting on a sender

Process Summary

After simulation ends, a summary line is printed per process:

| <finish_time> | Proc <Node>.<PID> | Run <ticks>, Block <ticks>, Wait <ticks>, Sends <count>, Recvs <count>


Example:

| 00005 | Proc 01.01 | Run 2, Block 0, Wait 0, Sends 1, Recvs 1
| 00005 | Proc 01.02 | Run 2, Block 0, Wait 1, Sends 1, Recvs 1
| 00005 | Proc 02.01 | Run 2, Block 0, Wait 0, Sends 0, Recvs 2
| 00005 | Proc 02.02 | Run 2, Block 0, Wait 1, Sends 2, Recvs 0


This provides a clear breakdown of:

Total run time

Time spent blocked

Time waiting in the ready queue

Number of sends and receives performed