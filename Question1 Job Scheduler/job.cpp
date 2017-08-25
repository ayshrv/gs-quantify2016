/* Job Scheduling
 *
 * DOCUMENTATION
 *
 * Assumptions:
 * 1. Priority of no 2 jobs will ever be equal (given).
 *
 * Data Structures Used:
 * + The system has the following data structures:
 *   -- A map of unique_id to each job.
 *      -> Efficient lookup by unique_id (assigned by us).
 *      -> An binary tree was used instead of a hash-map to avoid
 *         performance degradation due to the nature of memory (paging, cache locality).
 *   -- A Bi-directional map to store instructions and originating string and an
 *      identifier integer, for efficient access from both key and value.
 *      -> A hash-map is used instead of a binary tree map to avoid overheads
 *         and because the size of the tree is given to not be too large.
 *   -- A priority queue of jobs currently waiting in the queue.
 *      -> Build-in abstract priority_queue is a heap, sorted in the
 *         order of increasing priority.
 *      -> Best data structure for a priority queue, O(1) lookup,
 *         O(lg n) insertion & deletion.
 *   -- A priority queue of timestamps of busy CPUs (the timestamp of when they
 *      are supposed to be finished processing), another min-heap.
 *   -- A vector of (time_of_arrival, unique_id) corresponding to each job.
 *      -> Insertion is performed when a new job is added.
 *      -> The vector is sorted by timestamp, search can be done in O(log n).
 *   -- A vector of (timestamp, state_of_queue), which stores the state of queue
 *      corresponding to the timestamp at each "assign" operation.
 *      -> The state is stored only if the time difference between the new assign statement
 *         and the last assign is greater than a fixed interval of time.
 *      -> Since the insertion is done in an ordered manner according to timestamp,
 *         lookup can be done in O(log n).
 *   -- A vector of (timestamp, num_free_CPUs).
 *      -> It stores the number of free CPUs available at the time of each assign statement call.
 *   -- Vectors are used here because the order of timestamp of "assign" and "job" commands is
 *      guranteed to be non-deceasing, therefore insertion can be done is O(1) and lookup in O(log n)
 *      which is very-efficient due to having almost no overhead due to container use.
 *  *** The (timestamp, unique_id) and (timestamp, num_free_cpus) is merged
 *  into one vector for convenience of processing queries.***
 *
 * Algorithm:
 * + "job" command:    the job is inserted into the map, the job vector and the job queue.
 * + "assign" command: the free CPUs are collected, min(k, f, s) jobs are popped from
 *                      the job queue and assigned. The state of the job queue at this
 *                      timestamp is stored if the difference of timestamp exceeds a
 *                      given limit. Otherwise, only the number of free CPUs is stored.
 * + "query" command:  for a given timestamp, the job queue history vector is searched for the
 *                      timestamp just lesser than the one required, then the state
 *                      is evaluated and the required jobs is calculated based on how
 *                      many jobs lie in the obtained time interval.
 *
 * Space Complexity:
 *      No. of jobs = j
 *      No. of assign commands = k
 *      No. of CPUs = n
 *
 *      Space Complexity = O(n + kj)
 *
 * Time Complexity:
 *      No. of jobs = j
 *      No. of assign commands = k
 *      No. of CPUs = n
 *
 *  + "job" : O(log j)
 *  + "assign" : O(n + jlog j)
 *  + "query <k>" : O(log k + jlog j)
 *  + "query <orig>" : O(log k + jlog j)
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>
#include <queue>
#include <cctype>

// Declarations of System Structures and State Variables

// Defines
#define TIME_INTERVAL 50
#define ASSIGN_ID 1
#define JOB_ID 2

// Tupe definitions
typedef uint64_t time_stamp;
typedef uint64_t p_id;

/* Job structure.
 * Contains all the information pertinent to each job object.
 */
struct job {
    p_id unique_id;           // Self-assigned unique id for job
    p_id pid;
    time_stamp timestamp;
    int orig;                 // Orig identifier
    int instructions;         // Instructions indetifier
    int importance;
    uint32_t duration;
};

/* A Bimap class
 * It's a bi-directional hash-map where values can be
 * accessed quickly from both the key, value pairs.
 *
 * It ensures that (key, value) pairs are unique.
 */
template <typename n, typename m>
class bimap {
  private:
    std::unordered_map< n, m > map_f;       // Forward map
    std::unordered_map< m, n > map_b;       // Backward map

  public:
    // Add the pair to bimap only if neither exists already
    void add (n x1, m x2) {
        if (!exists (x1) && !exists (x2)) {
            map_f[x1] = x2;
            map_b[x2] = x1;
        }
    }

    // Check whether such a key exists.
    bool exists (n x) const {
        if (map_f.find (x) == map_f.end ())
            return false;

        return true;
    }

    // Check whether such a key exists.
    bool exists (m x) const {
        if (map_b.find (x) == map_b.end ())
            return false;

        return true;
    }

    // Return value from key
    m operator[] (n x) {
        return map_f[x];
    }

    // Return key from value
    n operator[] (m x) {
        return map_b[x];
    }
};

// Hash from pid to job description
std::vector< job > jobs;

// Bimaps for instruction set and origin set
bimap< std::string, int > instruction_set, orig_set;

// COunters for indentifying integer for instruction and orig
int inst_counter = 0, orig_counter = 0;

// Global counter for job unique_id
p_id counter = 1;

// A vector of (timestamp, pid) for each job
std::vector< std::pair< time_stamp, std::pair< p_id , int > > > job_timestamp;

/* Decide the priority order through the comparator struct.
 * The job with 'lesser' value is regarded as being of
 * higher priority.
 *
 * If lhs has higher priority then lhs > rhs is true.
 */
struct job_comparator {
    bool operator() (const p_id& l, const p_id& r) {
        job lhs = jobs[l],
            rhs = jobs[r];

        if (lhs.importance != rhs.importance)
            return lhs.importance < rhs.importance ? true : false;
        else {
            if (lhs.timestamp != rhs.timestamp)
                return lhs.timestamp > rhs.timestamp ? true : false;
            else {
                return lhs.duration > rhs.duration ? true : false;
            }
        }
    }
};

// Queue to store the pid of incoming job at the time of input for history
std::priority_queue< p_id, std::vector< p_id >, job_comparator > job_queue;

// Vector to store the state of job queue in the form of
// a pair of (timestamp, queue at that time) at each assign call
std::vector< std::pair< time_stamp,
    std::priority_queue< p_id,
                         std::vector< p_id >,
                         job_comparator > > > job_queue_history;

// Number of CPUs
uint64_t num_cpus;

// Queue holds the timestamp of busy CPUs
std::priority_queue< time_stamp,
    std::vector< time_stamp >,
    std::greater< time_stamp > > cpu_queue;

/* A generic binary search function
 * to search a given pair of vector
 * for index of the of a timestamp,
 * which can be later used to retrieve the
 * other member of the pair.
 *
 * If not found, It return the index that the query would be at
 * if it were in the vector.
 * T(n) = O(log n)
 */
template <typename T>
size_t binary_search (std::vector< std::pair< uint64_t, T > > v,
        uint64_t ts) {
    size_t l = 0, u = v.size () - 1, m, idx = 0;

    while (l <= u) {
        m = l + ((u - l) / 2);

        if (v[m].first == ts) {
            idx = m;
            l = m + 1;
        }
        else if (v[m].first < ts) {
            l = m + 1;
            idx = m;
        }
        else {
            u = m - 1;
        }
    }

    return idx;
}


// main function -- code entry-point.
int main (void) {
    
    std::ios::sync_with_stdio(0); std::cin.tie(0);std::cout.tie(0);
    // Temporary variables
    
    std::string input, temp;
    time_stamp ts;
    p_id t_uid, t_pid;
    uint64_t k, n;
    std::string orig, instruction;
    job temp_job;

    // Initialise the system
    job_queue_history.push_back (std::make_pair (0, job_queue));
    job_timestamp.push_back (std::make_pair (0, std::make_pair(0, 1)));
    jobs.push_back (temp_job);

    //Begin input
    std::cin >> temp;

    // Input the number of CPUs
    if (temp == "cpus") {
        std::cin >> num_cpus;
    }

    // Loop while EOF is not encountered
    while (std::getline (std::cin, input)) {

        std::stringstream s_stream (input);  // Create a string stream from line
        s_stream >> temp;

        if (temp == "job") {
            //Input job details
            s_stream >> temp_job.timestamp
                     >> temp_job.pid
                     >> orig
                     >> instruction
                     >> temp_job.importance
                     >> temp_job.duration;

            // If instruction already exists in the set, get the id and assign
            if (instruction_set.exists (instruction))
                temp_job.instructions = instruction_set[instruction];
            // Otherwise, add to the set and assign new id
            else {
                instruction_set.add (instruction, inst_counter);
                temp_job.instructions = inst_counter;
                inst_counter++;
            }

            // If orig already exists in the set, get the id and assign
            if (orig_set.exists (orig))
                temp_job.orig = orig_set[orig];
            // Otherwise, add to the set and assign new id
            else {
                orig_set.add (orig, orig_counter);
                temp_job.orig = orig_counter;
                orig_counter++;
            }

            // Assign a unique id to job
            temp_job.unique_id = counter++;

            // Perform the "job <details>" command, store the jobs in a job map
            jobs.push_back (temp_job);
            job_timestamp.push_back (std::make_pair (temp_job.timestamp, std::make_pair(temp_job.unique_id, JOB_ID)));
            job_queue.push (temp_job.unique_id);
        }
        else if (temp == "assign") {
            // Input assign details
            s_stream >> ts >> k;

            // Perform the "assign <timestamp> <k>" command
            // Remove completed jobs from the CPU queue
            while (!cpu_queue.empty () && cpu_queue.top () <= ts)
                cpu_queue.pop ();

            uint64_t free_cpus = num_cpus - cpu_queue.size (),
                     num_waiting = job_queue.size (),
                     num_processed = std::min (k, std::min (free_cpus, num_waiting));

            // Loop min(k, f, s) times
            for (uint64_t i = 0; i < num_processed; i++) {
                // Get the highest priority job from the queue
                t_uid = job_queue.top ();
                temp_job = jobs[t_uid];
                job_queue.pop ();

                // Print the info for assigned job
                std::cout << "job"
                          << " " << temp_job.timestamp
                          << " " << temp_job.pid
                          << " " << orig_set[temp_job.orig]
                          << " " << instruction_set[temp_job.instructions]
                          << " " << temp_job.importance
                          << " " << temp_job.duration << "\n";

                // Push the job in the CPU processing queue
                cpu_queue.push (ts + temp_job.duration);
            }

            // Push the details of this assign call in the job queue history vector
            if(ts-job_queue_history.back().first >= TIME_INTERVAL)
                job_queue_history.push_back (std::make_pair (ts, job_queue));
            else
                job_timestamp.push_back (std::make_pair (ts, std::make_pair(num_processed, ASSIGN_ID)));

        }
        else if (temp == "query") {
            // Input query details
            s_stream >> ts >> orig;

            // Check for the type of query
            bool isnum = true;
            for (auto& a : orig) {
                if (!isdigit (a)) {
                    isnum = false;
                    break;
                }
            }

            // Calculate lower bound indices for job and queue_history vectors
            size_t just_before_idx = binary_search (job_queue_history, ts),
                   job_idx = binary_search (job_timestamp,
                                            job_queue_history[just_before_idx].first);

            // The state of job_queue at the last assign call
            auto result = job_queue_history[just_before_idx].second;

            // We want the next job from queue
            job_idx++;

            // Loop while the timestamp of jobs is less than given timestamp
            while (job_idx < job_timestamp.size() && job_timestamp[job_idx].first <= ts) {
                // Push the jobs (as they are unassigned)
                if(job_timestamp[job_idx].second.second == JOB_ID)
                    result.push (job_timestamp[job_idx].second.first);

                // For assign statement, pop the no. of jobs that is equal
                // to the no. of free CPUs
                else /*if (job_timestamp[job_idx].second.second == ASSIGN_ID)*/ {
                    n = job_timestamp[job_idx].second.first;

                    for(int i = 0; i < n; i++)
                        result.pop();
                }

                job_idx++;
            }

            if (isnum) {
                // Perform the "query <timestamp> <k>" command
                k = std::stoi (orig);

                // Loop min(s, k) times
                while (!result.empty () && k--) {
                    // Obtain job details from job map
                    temp_job = jobs[result.top ()];

                    result.pop ();

                    std::cout << "job"
                          << " " << temp_job.timestamp
                          << " " << temp_job.pid
                          << " " << orig_set[temp_job.orig]
                          << " " << instruction_set[temp_job.instructions]
                          << " " << temp_job.importance
                          << " " << temp_job.duration << "\n";
                }
            }
            else {
                // Perform the "query <timestamp> <orig>" command
                while (!result.empty ()) {
                    // Obtain job details from job map
                    temp_job = jobs[result.top ()];
                    result.pop ();

                    if (orig_set[temp_job.orig] == orig) {
                        std::cout << "job"
                              << " " << temp_job.timestamp
                              << " " << temp_job.pid
                              << " " << orig_set[temp_job.orig]
                              << " " << instruction_set[temp_job.instructions]
                              << " " << temp_job.importance
                              << " " << temp_job.duration << "\n";
                    }
                }
            }
        }
    }

    return 0;
}
