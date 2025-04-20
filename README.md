# SIGA - Integrated Student Management System Simulation

This project simulates an **Integrated Student Management System (SIGA)** where students submit support requests and support agents handle those requests. It was developed for the **Operating Systems** course (Universidade Lusófona, 2024/2025) and spans three practical assignments.

## 🎯 Objective

- Simulate a real-world process involving inter-process communication.
- Apply core concepts from Operating Systems:
  - Named Pipes (FIFO)
  - Bash Scripting
  - C Programming
  - Thread Management and Synchronization (pthreads)
  - Process Coordination and Communication

---

## 🧪 Practical Assignments

### 📘 Practical Assignment 1 - Basic Communication

> Implements a simple system where students submit requests and a support agent handles them via a single named pipe.

#### Components
- `student.c`: Written in C, sends a request to the named pipe.
- `suporte_agente.sh`: Bash script that reads and processes requests.
- `suporte_desk.sh`: Main controller script that creates the pipe, launches the processes, and cleans up.

#### Improvements Implemented (Optional)
- Argument validation
- Dynamic number of students via command-line
- Custom pipe names
- Randomized agent wait time (1-5 seconds)

---

### 📘 Practical Assignment 2 - Threaded Support Agent

> Introduces multithreading and more advanced communication. Students enroll in subject schedules via requests handled by a threaded support agent.

#### Components
- `student.c`: Sends a registration request and waits for a response.
- `support_agent.c`: C program with multiple threads handling requests and managing schedule data.
- `support_desk.sh`: Launches multiple students and the support agent based on command-line arguments.

#### Key Features
- Shared data structure for disciplines and schedule slots
- Thread synchronization using mutexes
- Bidirectional communication (each student has a personal response pipe)

---

### 📘 Practical Assignment 3 - Admin Interaction & CSV Export

> Expands the system with an admin interface and multiple functionalities such as report generation and shutdown requests.

#### New Component
- `admin.c`: C program that interacts with the support agent to request data, export files, and terminate the system.

#### New Functionalities
- Registering students in multiple disciplines (5 per student)
- Requesting schedules by student
- Exporting registrations to a CSV file
- Sending shutdown signals to the agent
- Admin handled via a separate named pipe and dedicated thread

---

## 🛠️ Technologies Used

- **C** (with POSIX Threads)
- **Bash Scripting**
- **Named Pipes (FIFO)**
- **Thread Synchronization (Mutex, RW Locks)**
- **Linux Environment** (WSL or Ubuntu VM)
- **GDB** (for debugging)

---

## 🖼️ Suggested Screenshots for GitHub

- Terminal output from `support_desk.sh` execution
- Terminal view of a student being registered
- CSV file sample exported by `admin.c`
- Output of `admin.c` menu interaction
- (Optional) Diagram showing pipe communication between processes

---

## 🚀 How to Run

1. Compile the C programs:
   ```bash
   gcc -o student student.c -lpthread
   gcc -o support_agent support_agent.c -lpthread
   gcc -o admin admin.c -lpthread
