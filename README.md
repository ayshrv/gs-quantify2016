# gs-quantify2016
Questions attempted in GS Quantify conducted by Goldman Sachs

--------

### Question 1 : Job Scheduler

#### Assumptions:
1. Priority of no 2 jobs will ever be equal (given).

#### Data Structures Used:
The system has the following data structures:
1. A map of `unique_id` to each job.
	* Efficient lookup by `unique_id` (assigned by us).
	* An binary tree was used instead of a hash-map to avoid performance degradation due to the nature of memory (paging, cache locality).

2. A Bi-directional map to store instructions and originating string and an identifier integer, for efficient access from both key and value.
   	* A hash-map is used instead of a binary tree map to avoid overheads and because the size of the tree is given to not be too large. 
3. A priority queue of jobs currently waiting in the queue.
	* Build-in abstract priority_queue is a heap, sorted in the order of increasing priority.
 	* Best data structure for a priority queue, O(1) lookup, O(lg n) insertion & deletion.
4. A priority queue of timestamps of busy CPUs (the timestamp of when they are supposed to be finished processing), another min-heap.
5. A vector of (`time_of_arrival`, `unique_id`) corresponding to each job.
 	* Insertion is performed when a new job is added.
 	* The vector is sorted by `timestamp`, search can be done in O(log n).
6. A vector of (`timestamp`, `state_of_queue`), which stores the state of queue corresponding to the timestamp at each "assign" operation.
 	* The state is stored only if the time difference between the new assign statement and the last assign is greater than a fixed interval of time.
 	* Since the insertion is done in an ordered manner according to timestamp, lookup can be done in O(log n). 
7. A vector of (`timestamp`, `num_free_CPUs`).
 	* It stores the number of free CPUs available at the time of each assign statement call.
8. Vectors are used here because the order of timestamp of "assign" and "job" commands is guranteed to be non-deceasing, therefore insertion can be done is O(1) and lookup in O(log n) which is very-efficient due to having almost no overhead due to container use.

The (`timestamp`, `unique_id`) and (`timestamp`, `num_free_cpus`) is merged into one vector for convenience of processing queries.

#### Algorithm:
	 "job" command:    the job is inserted into the map, the job vector and the job queue.
	 "assign" command: the free CPUs are collected, min(k, f, s) jobs are popped from the job queue and assigned. The state of the job queue at this timestamp is stored if the difference of timestamp exceeds a given limit. Otherwise, only the number of free CPUs is stored.
	 "query" command:  for a given timestamp, the job queue history vector is searched for the timestamp just lesser than the one required, then the state is evaluated and the required jobs is calculated based on how many jobs lie in the obtained time interval.

#### Space Complexity:
	 No. of jobs = j
	 No. of assign commands = k
	 No. of CPUs = n
	
Space Complexity = **O(n + kj)**

#### Time Complexity:
	 No. of jobs = j
	 No. of assign commands = k
	 No. of CPUs = n

"job" : **O(log j)**

"assign" : **O(n + jlog j)**

"query \<k\>" : **O(log k + jlog j)**

"query \<orig\>" : **O(log k + jlog j)**
