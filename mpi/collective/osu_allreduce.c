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
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0, stddev= 0.0, stddev1 =0.0,  quartiles[5], quartiles1[5];
    float *sendbuf, *recvbuf;
    int po_ret;
    size_t bufsize;
    FILE *log_file = stdout;
    char *str;
    MPI_Comm sub_comm, tmp_comm;
    int sub_rank, sub_numprocs, size_loop = 0, k = 0;
    double *avg_time_stats, *min_time_stats, *max_time_stats, *stddev_stats, *quartiles_stats;

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

    /* create sub communicator */
    if (options.num_comms == 1) {
        sub_comm = MPI_COMM_WORLD;
    }
    else {
	    sub_comm = get_my_sub_communicator(rank, numprocs);
    }

    MPI_Comm_rank(sub_comm, &sub_rank);
    MPI_Comm_size(sub_comm, &sub_numprocs);

    if (sub_rank == 0) {
        /* count how many size loop */
        for(size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {
            size_loop++;
        }

        avg_time_stats = (double *)malloc(sizeof(double) * size_loop);
        min_time_stats = (double *)malloc(sizeof(double) * size_loop);
        max_time_stats = (double *)malloc(sizeof(double) * size_loop);
	stddev_stats = (double *)malloc(sizeof(double) * size_loop * 2);
	quartiles_stats = (double *)malloc(sizeof(double) * size_loop * 10);
    }

    for(size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {

        if(size > LARGE_MESSAGE_SIZE) {
            options.skip = options.skip_large;
            options.iterations = options.iterations_large;
        }

        MPI_Barrier(MPI_COMM_WORLD);
	if (sub_comm !=  MPI_COMM_WORLD)
		MPI_Barrier(sub_comm);

        timer=0.0;
        for(i=0, j= 0; i < options.iterations + options.skip ; i++) {
            t_start = MPI_Wtime();
            MPI_Allreduce(sendbuf, recvbuf, size, MPI_FLOAT, MPI_SUM, sub_comm );
            t_stop=MPI_Wtime();
            if(i>=options.skip){

               timer+=t_stop-t_start;
               iter_time[j++] = (t_stop - t_start);
            }
            MPI_Barrier(sub_comm);
        }
        latency = (double)(timer * 1e6) / options.iterations;

#if 1
       print_coll_iterations_perf_data(iter_time, sub_comm, (int )(size * sizeof(float)), options.iterations, &stddev, quartiles, log_file);
       calculate_stats((timer / options.iterations), sub_comm, &stddev1, quartiles1);
#endif
        if (sub_rank == 0) {
            MPI_Reduce(&latency, &min_time_stats[k], 1, MPI_DOUBLE, MPI_MIN, 0,
                       sub_comm);
            MPI_Reduce(&latency, &max_time_stats[k], 1, MPI_DOUBLE, MPI_MAX, 0,
                       sub_comm);
            MPI_Reduce(&latency, &avg_time_stats[k], 1, MPI_DOUBLE, MPI_SUM, 0,
                       sub_comm);
            avg_time_stats[k] = avg_time_stats[k]/sub_numprocs;
	    stddev_stats[k*2 + 0] = stddev;
	    stddev_stats[k*2 + 1] = stddev1;
	    quartiles_stats[k*10 + 0] =  quartiles[0];
	    quartiles_stats[k*10 + 1] =  quartiles[1];
	    quartiles_stats[k*10 + 2] =  quartiles[2];
	    quartiles_stats[k*10 + 3] =  quartiles[3];
	    quartiles_stats[k*10 + 4] =  quartiles[4];

	    quartiles_stats[k*10 + 5] =  quartiles1[0];
	    quartiles_stats[k*10 + 6] =  quartiles1[1];
	    quartiles_stats[k*10 + 7] =  quartiles1[2];
	    quartiles_stats[k*10 + 8] =  quartiles1[3];
	    quartiles_stats[k*10 + 9] =  quartiles1[4];

            k++;
        }
        else {
            MPI_Reduce(&latency, NULL, 1, MPI_DOUBLE, MPI_MIN, 0,
                       sub_comm);
            MPI_Reduce(&latency, NULL, 1, MPI_DOUBLE, MPI_MAX, 0,
                       sub_comm);
            MPI_Reduce(&latency, NULL, 1, MPI_DOUBLE, MPI_SUM, 0,
			       sub_comm);
        }

       // print_stats_new(rank, size * sizeof(float), avg_time, min_time, max_time, stddev, quartiles);

	if (sub_comm !=  MPI_COMM_WORLD)
		MPI_Barrier(sub_comm);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) print_preamble(sub_rank);
    for (i = 0; i < numprocs; i++) {
        if (i == rank && sub_rank == 0) {
	    usleep(rank * 1000);
            printf("#sub communicator #%d (start rank:%d size:%d)\n", rank % sub_numprocs, rank, sub_numprocs);

            k = 0;
            for (size=options.min_message_size; size*sizeof(float) <= options.max_message_size; size *= 2) {
                print_stats_new(sub_rank, size * sizeof(float),
                            avg_time_stats[k], min_time_stats[k], max_time_stats[k], &stddev_stats[k*2], &quartiles_stats[k*10]);
                k++;
            }
            printf("\n");
        }
    }

    if (sub_rank == 0) {
        free(avg_time_stats);
        free(min_time_stats);
        free(max_time_stats);
    }

    /* free sub communicator */
    if (options.num_comms != 1) {
        MPI_Comm_free(&sub_comm);
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
