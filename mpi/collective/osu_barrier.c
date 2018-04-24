#define BENCHMARK "OSU MPI%s Barrier Latency Test"
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
    int i = 0, rank, j;
    int numprocs;
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0, stddev[2],  quartiles[10];
    double latency = 0.0, t_start = 0.0, t_stop = 0.0;
    double timer=0.0, *iter_time;
    int po_ret;
    MPI_Comm sub_comm, tmp_comm;
    int sub_rank, sub_numprocs;

    set_header(HEADER);
    set_benchmark_name("osu_barrier");
    enable_accel_support();
    po_ret = process_options(argc, argv);

    if (po_okay == po_ret && none != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    options.show_size = 0;

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
        if(rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_Finalize();

        return EXIT_FAILURE;
    }


    options.skip = options.skip_large;
    options.iterations = options.iterations_large;
    timer = 0.0;

    iter_time = (double *) malloc(sizeof(double) * options.iterations);
    if (!iter_time) {
       fprintf(stderr, "Failed to allocate \n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    /* create sub communicator */
    if (options.num_comms == 1) {
        sub_comm = MPI_COMM_WORLD;
    }
    else {

	sub_comm = get_my_sub_communicator(rank, numprocs);
    }

    MPI_Comm_rank(sub_comm, &sub_rank);
    MPI_Comm_size(sub_comm, &sub_numprocs);

    MPI_Barrier(MPI_COMM_WORLD);

    for(i=0, j = 0; i < options.iterations + options.skip ; i++) {
        t_start = MPI_Wtime();
        MPI_Barrier(sub_comm);
        t_stop = MPI_Wtime();

        if(i>=options.skip){
            timer+=t_stop-t_start;
               iter_time[j++] = (t_stop - t_start);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    latency = (timer * 1e6) / options.iterations;
       print_coll_iterations_perf_data(iter_time, sub_comm, 0, options.iterations, &stddev[0], &quartiles[0], NULL);
       calculate_stats((timer / options.iterations), sub_comm, &stddev[1], &quartiles[5]);


    MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
                sub_comm);
    MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
		sub_comm);
    MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
                sub_comm);
    avg_time = avg_time/sub_numprocs;

    if (rank == 0) print_preamble(sub_rank);
    MPI_Barrier(MPI_COMM_WORLD);
    for (i = 0; i < numprocs; i++) {
        if (i == rank && sub_rank == 0) {
	    usleep(rank * 1000);
            printf("#sub_communicator-#%d start_rank:%d size:%d\n", rank % sub_numprocs, rank, sub_numprocs);

                print_stats_new(sub_rank, 0, avg_time, min_time, max_time,  &stddev[0], &quartiles[0]);
            printf("\n");
        }
    }
    /* free sub communicator */
    if (options.num_comms != 1) {
        MPI_Comm_free(&sub_comm);
    }

    MPI_Finalize();

    return EXIT_SUCCESS;
}

/* vi: set sw=4 sts=4 tw=80: */
