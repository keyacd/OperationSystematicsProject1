#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "msg.h"
#include "out.h"

// Darien Keyack (661190088) and Corwin Aucoin (661178786)

void reset(int *t, int *ready_n, int *waiting_n, int n, bool *running_active, int *blocked_n) {
    msg_space();
    *t = 0;
    *ready_n = 0;
    *waiting_n = n;
    running_active = false;
    blocked_n = 0;
}

void sort(struct Process *array, int arr_size) {
    int i;
    int j;
    struct Process temp;
    for (i = 1; i < arr_size; i++) {
        j = i;
        while (j > 0 && array[j - 1].burst_left > array[j].burst_left) {
            temp = array[j - 1];
            array[j - 1] = array[j];
            array[j] = temp;
            j--;
        }
    }
}

int main(int argc, char *argv[]) {
    // Open input file
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) msg_error("Invalid input file format");
    
    // Open output file
    FILE *output = fopen(argv[2], "w");
    if (output == NULL) msg_error("Invalid output file format");
    
    // Simulation Configuration
    int n = 0; // number of processes to simulate; will be determined via input file
    //int m = 1; // number of processors (i.e. cores) available w/in the CPU
    int t_cs = 6; // time (in ms) it takes to perform a context switch
    int t_slice = 94; // time slice (in ms) for RR
    #ifdef DEBUG
        char limit = 'F';
        if (argc > 3) limit = toupper(argv[3][0]);
    #endif
    
    // Get all lines from file
    char line[100];
    int size_array = 3;
    char **array_raw = (char**) calloc(size_array, sizeof(char*));
    while (fgets(line, sizeof(line), input) != NULL) {
        if (line[0] != '#' && line[0] != ' ' && line[0] != '\n') {
            if (n >= size_array) {
                size_array = size_array * 2;
                array_raw = realloc(array_raw, size_array * sizeof(char*));
                if (array_raw == NULL) msg_error("memory for array_raw not re-allocated");
            }
            array_raw[n] = malloc(strlen(line) + 1);
            strcpy(array_raw[n], line);
            n += 1;
        }
    }
    fclose(input);
    
    // Create Processes
    struct Process *array = (struct Process*) calloc(n, sizeof(struct Process));
    int i;
    int j;
    int init;
    for (i = 0; i < n; i++) {
        array[i].id = array_raw[i][0];
        j = 2;
        array[i].arrive = next(&j, array_raw[i]);
        array[i].burst_time = next(&j, array_raw[i]);
        array[i].burst_num = next(&j, array_raw[i]);
        // Problem: when trying to set io with next, *** stack smashing detected ***
        init = j;
        strcpy(line, "");
        while (array_raw[i][j] != '|' && j < strlen(array_raw[i])) {
            line[j - init] = array_raw[i][j];
            line[j - init + 1] = '\0';
            j++;
        }
        array[i].io = atoi(line);
        free(array_raw[i]);
    }
    free(array_raw);
    for (i = 0; i < n; i++) {
        array[i].burst_left = array[i].burst_time;
        array[i].arrive_wait = array[i].arrive;
        array[i].arrive_turn = array[i].arrive;
    }
    
    // Calculate average CPU burst time
    int burst_total = 0;
    int burst_count = 0;
    for (i = 0; i < n; i++) {
        burst_total += (array[i].burst_time * array[i].burst_num);
        burst_count += array[i].burst_num;
    }
    float burst = avg(burst_total, burst_count);
    
    // First Come First Serve (FCFS)
    int t = 0;
    struct Process *ready = (struct Process*) calloc(n, sizeof(struct Process));
    int ready_n = 0;
    struct Process *waiting = (struct Process*) calloc(n, sizeof(struct Process));
    int waiting_n = n;
    for (i = 0; i < n; i++) waiting[i] = array[i];
    struct Process running;
    bool running_active = false;
    struct Process *blocked = (struct Process*) calloc(n, sizeof(struct Process));
    int blocked_n = 0;
    bool add_cs;
    int wait_total = 0;
    int wait_count = 0;
    int turnaround_total = 0;
    int turnaround_count = 0;
    int switches = 0;
    msg_sim_start(t, "FCFS", ready, ready_n);
    t--;
    
    while (ready_n > 0 || waiting_n > 0 || blocked_n > 0 || running_active) {
        add_cs = false;
        // Set running, if possible/none already
        if (!running_active && ready_n > 0) { 
            running = ready[0];
            running_active = true;
            ready_n--;
            for (i = 0; i < ready_n; i++) ready[i] = ready[i + 1];
            t += t_cs/2;
            msg_cpu(t, running, ready, ready_n);
            switches++;
            running.arrive = t + running.burst_time;
            wait_total += t - running.arrive_wait - t_cs/2;
            wait_count++;
        }
        // Update time if nothing else has been done this tick
        else {
            t++;
            // Update running, if possible
            if (running_active && running.arrive <= t) {
                running.burst_num--;
                running_active = false;
                add_cs = true;
                // Add to blocked, if possible
                if (running.burst_num > 0) {
                    msg_burst(t, running, ready, ready_n);
                    running.arrive = t + t_cs/2 + running.io;
                    msg_block(t, running, ready, ready_n);
                    blocked_n++;
                    blocked[blocked_n - 1] = running;
                }
                // Terminate if finished
                else msg_event_q(t, running.id, "terminated", ready, ready_n);
                turnaround_count++;
                turnaround_total += t + t_cs/2 - running.arrive_turn;
            }
        }
        // Check for I/O completion
        for (i = 0; i < blocked_n; ++i) {
            if (blocked[i].arrive <= t) {
                ready_n++;
                ready[ready_n - 1] = blocked[i];
                msg_added_ready(t, ready[ready_n - 1].id, "completed I/O;", ready, ready_n);
                ready[ready_n - 1].arrive_wait = t;
                ready[ready_n - 1].arrive_turn = t;
                blocked_n--;
                for (j = i; j < blocked_n; j++) blocked[j] = blocked[j + 1];
            }
        }
        // Check for new arrivals
        for (i = 0; i < waiting_n; ++i) {
            if (waiting[i].arrive <= t) {
                ready_n++;
                ready[ready_n - 1] = waiting[i];
                msg_added_ready(t, ready[ready_n - 1].id, "arrived and", ready, ready_n);
                ready[ready_n - 1].arrive_wait = t;
                ready[ready_n - 1].arrive_turn = t;
                waiting_n--;
                for (j = i; j < waiting_n; j++) waiting[j] = waiting[j + 1];
                i--;
            }
        }
        if (add_cs) t += t_cs/2;
    }
    msg_sim_end(t, "FCFS");
    out_params(argv[1], "FCFS", output, burst, wait_total, wait_count, turnaround_total, turnaround_count, switches, 0);
    #ifdef DEBUG
        if (limit == 'R') exit(EXIT_SUCCESS);
    #endif
    
    // Shortest Remaining Time (SRT)
    reset(&t, &ready_n, &waiting_n, n, &running_active, &blocked_n);
    wait_total = 0;
    wait_count = 0;
    turnaround_total = 0;
    turnaround_count = 0;
    switches = 0;
    int preempts = 0;
    for (i = 0; i < n; i++) waiting[i] = array[i];
    msg_sim_start(t, "SRT", ready, ready_n);
    t--;
    
    while (ready_n > 0 || waiting_n > 0 || blocked_n > 0 || running_active) {
        add_cs = false;
        // Set running, if possible/none already
        if (!running_active && ready_n > 0) {
            j = 0;
            for (i = 0; i < ready_n; i++) if (ready[i].burst_left < ready[j].burst_left) j = i;
            running = ready[j];
            running_active = true;
            ready_n--;
            for (i = j; i < ready_n; i++) ready[i] = ready[i + 1];
            t += t_cs/2;
            msg_cpu(t, running, ready, ready_n);
            switches++;
            running.arrive = t + running.burst_time;
            if (running.burst_left < running.burst_time) wait_total += t - running.arrive_wait;
            else wait_total += t - running.arrive_wait - t_cs/2;
            wait_count++;
        }
        // Update time if nothing else has been done this tick
        else {
            // Update running, if possible
            if (running_active && running.burst_left <= 0) {
                running.burst_left = 0;
                running.burst_num--;
                running_active = false;
                add_cs = true;
                // Add to blocked, if possible
                if (running.burst_num > 0) {
                    msg_burst(t, running, ready, ready_n);
                    running.arrive = t + t_cs/2 + running.io;
                    msg_block(t, running, ready, ready_n);
                    blocked_n++;
                    blocked[blocked_n - 1] = running;
                }
                // Terminate if finished
                else msg_event_q(t, running.id, "terminated", ready, ready_n);
                turnaround_count++;
                turnaround_total += t + t_cs/2 - running.arrive_turn;
            }
            else {
                t++;
                running.burst_left--;
                // Update running, if possible (again)
                if (running_active && running.burst_left <= 0) {
                    running.burst_left = 0;
                    running.burst_num--;
                    running_active = false;
                    add_cs = true;
                    // Add to blocked, if possible
                    if (running.burst_num > 0) {
                        msg_burst(t, running, ready, ready_n);
                        running.arrive = t + t_cs/2 + running.io;
                        msg_block(t, running, ready, ready_n);
                        blocked_n++;
                        blocked[blocked_n - 1] = running;
                    }
                    // Terminate if finished
                    else msg_event_q(t, running.id, "terminated", ready, ready_n);
                    turnaround_count++;
                    turnaround_total += t + t_cs/2 - running.arrive_turn;
                }
            }
        }
        // Check for I/O completion
        for (i = 0; i < blocked_n; ++i) {
            // Preemptive
            if (blocked[i].arrive <= t) {
                blocked[i].arrive_turn = t;
                if (blocked[i].burst_time < running.burst_left) {
                    msg_preempt(t, blocked[i].id, running.id, "completed I/O", ready, ready_n);
                    ready_n++;
                    ready[ready_n - 1] = running;
                    ready[ready_n - 1].arrive_wait = t;
                    sort(ready, ready_n);
                    running = blocked[i];
                    running.burst_left = running.burst_time;
                    t += t_cs;
                    msg_cpu(t, running, ready, ready_n);
                    switches++;
                    preempts++;
                    running.arrive = t + running.burst_time;
                    blocked_n--;
                    for(j = i; j < blocked_n; j++) blocked[j] = blocked[j + 1];
                }
                // Non-preemptive
                else {
                    ready_n++;
                    ready[ready_n - 1] = blocked[i];
                    ready[ready_n - 1].burst_left = ready[ready_n - 1].burst_time;
                    ready[ready_n - 1].arrive_wait = t;
                    sort(ready, ready_n);
                    msg_event_q(t, blocked[i].id, "completed I/O; added to ready queue", ready, ready_n);
                    blocked_n--;
                    for (j = i; j < blocked_n; j++) blocked[j] = blocked[j + 1];
                }
                
            }
        }
        // Check for new arrivals
        for (i = 0; i < waiting_n; ++i) {
            if (waiting[i].arrive <= t) {
                waiting[i].arrive_turn = t;
                // Preemptive
                if (running_active && waiting[i].burst_left < running.burst_left) {
                    msg_preempt(t, waiting[i].id, running.id, "arrived", ready, ready_n);
                    ready_n++;
                    ready[ready_n - 1] = running;
                    ready[ready_n - 1].arrive_wait = t;
                    sort(ready, ready_n);
                    t += t_cs;
                    running = waiting[i];
                    msg_cpu(t, running, ready, ready_n);
                    switches++;
                    preempts++;
                    running.arrive = t + running.burst_time;
                }
                // Non-preemptive
                else {
                    ready_n++;
                    ready[ready_n - 1] = waiting[i];
                    ready[ready_n - 1].arrive_wait = t;
                    sort(ready, ready_n);
                    msg_added_ready(t, ready[ready_n - 1].id, "arrived and", ready, ready_n);
                }
                waiting_n--;
                for (j = i; j < waiting_n; j++) waiting[j] = waiting[j + 1];
                i--;
            }
        }
        if (add_cs) t += t_cs/2;
    }
    msg_sim_end(t, "SRT");
    out_params(argv[1], "SRT", output, burst, wait_total, wait_count, turnaround_total, turnaround_count, switches, preempts);
    #ifdef DEBUG
        if (limit == 'S') exit(EXIT_SUCCESS);
    #endif

    // Round Robin (RR)
    reset(&t, &ready_n, &waiting_n, n, &running_active, &blocked_n);
    wait_total = 0;
    wait_count = 0;
    turnaround_total = 0;
    turnaround_count = 0;
    switches = 0;
    preempts = 0;
    for (i = 0; i < n; i++) waiting[i] = array[i];
    msg_sim_start(t, "RR", ready, ready_n);
    t--;
    
    while (ready_n > 0 || waiting_n > 0 || blocked_n > 0 || running_active) {
        add_cs = false;
        // Set running, if possible/none already
        if (!running_active && ready_n > 0) {
            running = ready[0];
            running_active = true;
            ready_n--;
            for (i = 0; i < ready_n; i++) ready[i] = ready[i + 1];
            t += t_cs/2;
            msg_cpu(t, running, ready, ready_n);
            switches++;
            running.arrive = t + t_slice;
            wait_total += t - running.arrive_wait - t_cs/2;
            if (running.burst_left >= running.burst_time) wait_count++;
        }
        
        // Update time if nothing else has been done this tick
        else {
            // Update running, if possible
            if (running_active && running.burst_left <= 0) {
                running.burst_left = 0;
                running.burst_num--;
                running_active = false;
                add_cs = true;
                // Add to blocked, if possible
                if (running.burst_num > 0) {
                    msg_burst(t, running, ready, ready_n);
                    running.arrive = t + t_cs/2 + running.io;
                    msg_block(t, running, ready, ready_n);
                    blocked_n++;
                    blocked[blocked_n - 1] = running;
                }
                // Terminate if finished
                else msg_event_q(t, running.id, "terminated", ready, ready_n);
                turnaround_count++;
                turnaround_total += t + t_cs/2 - running.arrive_turn;
            }
            else if (running_active && running.arrive <= t) {
                // Then the time slice is up, and it needs to be added to the back of the ready queue
                if (ready_n > 0) {
                    msg_slice_preempt(t, running.id, running.burst_left, ready, ready_n);
                    preempts++;
                    ready_n++;
                    ready[ready_n - 1] = running;
                    ready[ready_n - 1].arrive_wait = t;
                    running_active = false;
                    add_cs = true;
                }
                else {
                    running.arrive = t + t_slice;
                    msg_event_q(t, ' ', "Time slice expired; no preemption because ready queue is empty", ready, ready_n);
                }
            }
            else {
                t++;
                running.burst_left--;
                // Update running, if possible (again)
                if (running_active && running.burst_left <= 0) {
                    running.burst_left = 0;
                    running.burst_num--;
                    running_active = false;
                    add_cs = true;
                    // Add to blocked, if possible
                    if (running.burst_num > 0) {
                        msg_burst(t, running, ready, ready_n);
                        running.arrive = t + t_cs/2 + running.io;
                        msg_block(t, running, ready, ready_n);
                        blocked_n++;
                        blocked[blocked_n - 1] = running;
                    }
                    // Terminate if finished
                    else msg_event_q(t, running.id, "terminated", ready, ready_n);
                    turnaround_count++;
                    turnaround_total += t + t_cs/2 - running.arrive_turn;
                }
            }
        }
        // Check for I/O completion
        for (i = 0; i < blocked_n; ++i) {
            if (blocked[i].arrive <= t) {
                blocked[i].burst_left = blocked[i].burst_time;
                ready_n++;
                ready[ready_n - 1] = blocked[i];
                msg_added_ready(t, ready[ready_n - 1].id, "completed I/O;", ready, ready_n);
                ready[ready_n - 1].arrive_wait = t;
                ready[ready_n - 1].arrive_turn = t;
                blocked_n--;
                for (j = i; j < blocked_n; j++) blocked[j] = blocked[j + 1];
            }
        }
        // Check for new arrivals
        for (i = 0; i < waiting_n; ++i) {
            if (waiting[i].arrive <= t) {
                ready_n++;
                ready[ready_n - 1] = waiting[i];
                msg_added_ready(t, ready[ready_n - 1].id, "arrived and", ready, ready_n);
                ready[ready_n - 1].arrive_wait = t;
                ready[ready_n - 1].arrive_turn = t;
                waiting_n--;
                for (j = i; j < waiting_n; j++) waiting[j] = waiting[j + 1];
                i--;
            }
        }
        if (add_cs) t += t_cs/2;
    }
    msg_sim_end(t, "RR");
    out_params(argv[1], "RR", output, burst, wait_total, wait_count, turnaround_total, turnaround_count, switches, preempts);
    
    // Problem: can't just free(array), dunno how to deallocate it
    fclose(output);
    exit(EXIT_SUCCESS);
}