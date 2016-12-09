#define BENCHMARK "OSU MPI%s Allreduce Latency Test"
/*
 * Copyright (C) 2002-2016 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */
#include "osu_coll.h"

int main(int argc, char *argv[])
{
    int i, j, numprocs, rank, size;
    double latency = 0.0, t_start = 0.0, t_stop = 0.0;
    double timer=0.0, *iter_time;
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0, stddev= 0.0, quartiles[3];
    float *sendbuf, *recvbuf;
    int po_ret;
    size_t bufsize;
    FILE *log_file = stdout;
    char *str;

    set_header(HEADER);
    set_benchmark_name("osu_allreduce");
    enable_accel_support();
    po_ret = process_options(argc, argv);

    if (po_okay == po_ret && none != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    if ((str = getenv("LOG_FILE")) != NULL) {
	    log_file = fopen(str, "w");
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

    switch (po_ret) {
        case po_bad_usage:
            print_bad_usage_message(rank);
            MPI_Finalize();
            exit(EXIT_FAILURE);
        case po_help_message:
            print_help_message(rank);
            MPI_Finalize();
            exit(EXIT_SUCCESS);
        case po_version_message:
            print_version_message(rank);
            MPI_Finalize();
            exit(EXIT_SUCCESS);
        case po_okay:
            break;
    }

    if(numprocs < 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_Finalize();
        exit(EXIT_FAILURE);
    }
    

    if (options.max_message_size > options.max_mem_limit) {
        options.max_message_size = options.max_mem_limit;
    }

    options.min_message_size /= sizeof(float);
    if (options.min_message_size < DEFAULT_MIN_MESSAGE_SIZE) {
        options.min_message_size = DEFAULT_MIN_MESSAGE_SIZE;
    }

    bufsize = sizeof(float)*(options.max_message_size/sizeof(float));
    if (allocate_buffer((void**)&sendbuf, bufsize, options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    set_buffer(sendbuf, options.accel, 1, bufsize);

    bufsize = sizeof(float)*(options.max_message_size/sizeof(float));
    if (allocate_buffer((void**)&recvbuf, bufsize, options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    set_buffer(recvbuf, options.accel, 0, bufsize);

    iter_time = (double *) malloc(sizeof(double) * options.iterations);
    if (!iter_time) {
       fprintf(stderr, "Failed to allocate \n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    print_preamble(rank);

    for(size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {

        if(size > LARGE_MESSAGE_SIZE) {
            options.skip = options.skip_large;
            options.iterations = options.iterations_large;
        }

        MPI_Barrier(MPI_COMM_WORLD);

        timer=0.0;
        for(i=0, j= 0; i < options.iterations + options.skip ; i++) {
            t_start = MPI_Wtime();
            MPI_Allreduce(sendbuf, recvbuf, size, MPI_FLOAT, MPI_SUM, MPI_COMM_WORLD );
            t_stop=MPI_Wtime();
            if(i>=options.skip){

               timer+=t_stop-t_start;
               iter_time[j++] = (t_stop - t_start);
            }
            MPI_Barrier(MPI_COMM_WORLD);
        }
        latency = (double)(timer * 1e6) / options.iterations;

        MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
                MPI_COMM_WORLD);
        MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
                MPI_COMM_WORLD);
        MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
                MPI_COMM_WORLD);
        avg_time = avg_time/numprocs;

#if 1
       print_coll_iterations_perf_data(iter_time, rank, numprocs, (int )(size * sizeof(float)), options.iterations, &stddev, quartiles, log_file);
#endif

        print_stats_new(rank, size * sizeof(float), avg_time, min_time, max_time, stddev, quartiles);

        MPI_Barrier(MPI_COMM_WORLD);
    }

    free_buffer(sendbuf, options.accel);
    free_buffer(recvbuf, options.accel);

    MPI_Finalize();

    if (log_file != stdout) {
	    fclose(log_file);
    }

    if (none != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
