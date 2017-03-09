// Darien Keyack (661190088) and Corwin Aucoin (661178786)

float avg(int total, int count) {
    if (count == 0) return (float)total;
    else return ((float)total / (float)count);
}

void debug_float(float f, float v, FILE *output, char *algo, bool times) {
    float range = 0.01;
    if (times && algo != "FCFS") range = 2.0;
    if (v - f > range || f - v > range) fprintf(output, "** ERROR: correct value %.2f\n", f);
}

void debug_int(int i, int v, FILE *output) {
    if (i != v) fprintf(output, "** ERROR: correct value %i **\n", i);
}

void branch_float(float *f, char *algo, float fcfs, float srt, float rr) {
    if (algo == "FCFS") *f = fcfs;
    else if (algo == "SRT") *f = srt;
    else *f = rr;
}

void branch_int(int *i, char *algo, int fcfs, int srt, int rr) {
    if (algo == "FCFS") *i = fcfs;
    else if (algo == "SRT") *i = srt;
    else *i = rr;
}

void out_params(char *input, char *algo, FILE *output, float burst, int wait_total, int wait_count, int turnaround_total, int turnaround_count, int switches, int preempts) {
    fprintf(output, "Algorithm %s\n", algo);
    fprintf(output, "-- average CPU burst time: %.2f ms\n", burst);
    #ifdef DEBUG
        char c = input[9];
        float f;
        if (c == '1') f = 403.85;
        else if (c == '2') f = 100.00;
        else if (c == '3') f = 840.00;
        else if (c == '4') f = 102.50;
        else if (c == '5') f = 706.67;
        else if (c == '6') f = 47.83;
        debug_float(f, burst, output, algo, false);
    #endif
    float wait = avg(wait_total, wait_count);
    fprintf(output, "-- average wait time: %.2f ms\n", wait);
    #ifdef DEBUG
        if (c == '1') branch_float(&f, algo, 356.46, 140.38, 284.92);
        else if (c == '2') f = 0.00;
        else if (c == '3') branch_float(&f, algo, 1495.60, 794.80, 1715.87);
        else if (c == '4') branch_float(&f, algo, 327.67, 195.67, 507.67);
        else if (c == '5') branch_float(&f, algo, 1238.13, 505.60, 1067.33);
        else if (c == '6') branch_float(&f, algo, 10.42, 13.58, 10.42);
        debug_float(f, wait, output, algo, true);
    #endif
    float turnaround = avg(turnaround_total, turnaround_count);
    fprintf(output, "-- average turnaround time: %.2f ms\n", turnaround);
    #ifdef DEBUG
        if (c == '1') branch_float(&f, algo, 766.31, 551.85, 700.8);
        else if (c == '2') f = 106.00;
        else if (c == '3') branch_float(&f, algo, 2341.60, 1642.40, 2585.87);
        else if (c == '4') branch_float(&f, algo, 436.17, 304.17, 619.17);
        else if (c == '5') branch_float(&f, algo, 1950.80, 1220.07, 1798.80);
        else if (c == '6') branch_float(&f, algo, 64.25, 67.92, 64.25);
        debug_float(f, turnaround, output, algo, true);
    #endif
    fprintf(output, "-- total number of context switches: %i\n", switches);
    #ifdef DEBUG
        int i;
        if (c == '1') branch_int(&i, algo, 13, 20, 36);
        else if (c == '2') i = 5;
        else if (c == '3') branch_int(&i, algo, 15, 23, 135);
        else if (c == '4') branch_int(&i, algo, 30, 30, 60);
        else if (c == '5') branch_int(&i, algo, 15, 24, 109);
        else if (c == '6') branch_int(&i, algo, 12, 14, 12);
        debug_int(i, switches, output);
    #endif
    fprintf(output, "-- total number of preemptions: %i\n", preempts);
    #ifdef DEBUG
        i = 0;
        if (c == '1') branch_int(&i, algo, 0, 7, 23);
        else if (c == '3') branch_int(&i, algo, 0, 8, 120);
        else if (c == '4' && algo == "RR") i = 30;
        else if (c == '5') branch_int(&i, algo, 0, 9, 94);
        else if (c == '6' && algo == "SRT") i = 2;
        debug_int(i, preempts, output);
    #endif
}