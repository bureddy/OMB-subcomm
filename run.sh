#!/bin/bash

BASE_DIR=/hpc/scrap/users/devendar/sharp-coral-report
MPIRUN=/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/ompi-v2.x/bin/mpirun
BASE_FLAGS=""
now=$(date +"%m_%d_%Y_%I_%M")
#BASE_FLAGS+=" --report-bindings "
BASE_FLAGS="  --mca orte_leave_session_attached  1 -bind-to core  -mca pml yalla -mca btl_openib_warn_default_gid_prefix 0 -mca btl_openib_if_include mlx5_2:1 -x MXM_RDMA_PORTS=mlx5_2:1 -x HCOLL_MAIN_IB=mlx5_2:1 -x SHARP_COLL_LOG_LEVEL=2 -x HCOLL_ENABLE_MCAST_ALL=1 -x HCOLL_MCAST_NP=1 -x MLX5_SINGLE_THREADED=1 -x MXM_LOG_LEVEL=ERROR -x HCOLL_ML_DISABLE_REDUCE=1 -x LD_LIBRARY_PATH=/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/ompi-v2.x/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/sharp/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/fca/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/hcoll/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/ucx/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/mxm/lib:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/sharp/lib  -x LD_PRELOAD=/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/sharp/lib/libsharp.so:/hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/sharp/lib/libsharp_coll.so -x SHARP_COLL_MAX_PAYLOAD_SIZE=256 -x SHARP_COLL_JOB_QUOTA_PAYLOAD_PER_OST=256 -x HCOLL_SHARP_UPROGRESS_NUM_POLLS=999 -x HCOLL_BCOL_P2P_ALLREDUCE_SHARP_MAX=4096 -x SHARP_COLL_GROUP_RESOURCE_POLICY=1 -x SHARP_COLL_PIPELINE_DEPTH=32 -x SHARP_COLL_POLL_BATCH=1 -x SHARP_COLL_SHARP_ENABLE_MCAST_TARGET=1 -x SHARP_COLL_GROUP_IS_TARGET=1 -x SHARP_COLL_LOG_LEVEL=3 "

module load /hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/modulefiles/hpcx-ompi-v2.x
for sharp_enable in 3 0; do
    if [ "$sharp_enable" == "0" ]; then
        RUN_MODE="HOST"
    elif [ "$sharp_enable" == "3" ]; then
        RUN_MODE="SHArP"
    else
        echo "error in run mode"
        exit
    fi
        

for bench in allreduce barrier; do
    if [ "$bench" == "allreduce" ]; then
        EXE="/hpc/scrap/users/devendar/sharp-coral-report/osu-micro-benchmarks-5.2/mpi/collective/osu_allreduce -i 10000 -x 1000 -f "
        MAX_MSG_SIZE=" -m 4096"
    elif [ "$bench" == "barrier" ]; then
        EXE="/hpc/scrap/users/devendar/sharp-coral-report/osu-micro-benchmarks-5.2/mpi/collective/osu_barrier -i 10000 -x 2000 -f "
        MAX_MSG_SIZE=""
    else
        echo "wrong bench"
        exit
    fi     
          
    for nodes in 122 8 16 32 64; do
    #for nodes in 127; do
        HOST_FILE="$BASE_DIR/hfile$nodes"
        if [ ! -f $HOST_FILE ]; then
            echo "File not found!:$HOST_FILE"
            exit
        fi
        for ppn in 1 2 28; do
        #for ppn in 28; do
            NPROCS=$(($nodes*$ppn))
            EXE_PRE=""
            if [ "$ppn" == "1" ]; then
                PPN_FLAGS=" -mca rmaps_dist_device mlx5_2:1 -mca rmaps_base_mapping_policy dist:span -x MXM_ASYNC_INTERVAL=1800s"
                EXE_PRE="taskset -c 1 numactl --membind=0 "
            elif [ "$ppn" == "2" ]; then
                PPN_FLAGS=" -mca rmaps_dist_device mlx5_2:1 -mca rmaps_base_mapping_policy dist:span --cpu-set 1,15 -x MXM_ASYNC_INTERVAL=1800s "
            elif [ "$ppn" == "28" ]; then
                PPN_FLAGS=" -mca rmaps_dist_device mlx5_2:1 -mca rmaps_base_mapping_policy dist:span "
            fi
            
            for channels in 1 2; do
            #for channels in  2; do
                if [ "$ppn" == "1" ] && [ "$channels" == "2" ];
                then
                    continue
                fi
                if [ "$channels" == "2" ]; then
                    CHANNEL_FLAGS="-x SHARP_COLL_JOB_QUOTA_OSTS=64 -x HCOLL_BCOL=basesmuma,mlnx_p2p -x HCOLL_SBGP=basesmsocket,p2p "
                else
                    CHANNEL_FLAGS="-x SHARP_COLL_JOB_QUOTA_OSTS=128 "
                fi
                num_groups=1
                group_size=$(($num_groups*8))
                #while [ $group_size -le $nodes ]; do
                if [ $sharp_enable -gt 0 ]; then
                /hpc/scrap/users/devendar/sharp-coral-report/hpcx-gcc-redhat7.2/sharp/sbin/sharp_manager.sh -o restart
                fi
                while [ $group_size -le $(($nodes+1)) ]; do
                    OUTPUT_DIR=$BASE_DIR/DATA-$now/$RUN_MODE/$bench/nodes-$nodes/ppn-$ppn
                    if [[ ! -e $OUTPUT_DIR ]]; then
                        mkdir -p $OUTPUT_DIR
                    fi
                    EXE_FLAG=$MAX_MSG_SIZE;
                    if [ $num_groups == 8 ]; then
                        GROUP_SIZE_FLAGS="-x SHARP_COLL_JOB_QUOTA_MAX_GROUPS=16"
                        if [ "$bench" == "allreduce" ]; then
                            EXE_FLAG=" -m 2048"
                        fi
                    elif [ $num_groups == 16 ]; then
                        GROUP_SIZE_FLAGS="-x SHARP_COLL_JOB_QUOTA_MAX_GROUPS=32"
                        if [ "$bench" == "allreduce" ]; then
                            EXE_FLAG=" -m 1024"
                        fi
                    else
                        GROUP_SIZE_FLAGS="-x SHARP_COLL_JOB_QUOTA_MAX_GROUPS=$((1+$num_groups))"
                    fi
                    CMD="$MPIRUN -hostfile $HOST_FILE -np $NPROCS $PPN_FLAGS $CHANNEL_FLAGS $GROUP_SIZE_FLAGS $BASE_FLAGS -x HCOLL_ENABLE_SHARP=$sharp_enable  $EXE_PRE $EXE $EXE_FLAG -c $num_groups -p $ppn"
                    for count in 1 2 ; do
                        OUT_FILE=$OUTPUT_DIR/nodes-$nodes-ppn-$ppn-channels-$channels-groups-$num_groups-run_iter-$count.txt
                        echo $OUT_FILE
                        touch $OUT_FILE
                        echo     "$CMD" 2>&1  | tee $OUT_FILE
                       $CMD 2>&1  | tee -a $OUT_FILE
                    done
                    num_groups=$(($num_groups*2))
                    group_size=$(($num_groups*8))
                done
            done
        done
    done
done 
done 
