// this will test that MPI_Isend and MPI_Test work together
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  MPI_Request request = 0;
  int done = 0;

  // We are assuming at least 2 processes for this task
  if (world_size < 2) {
    fprintf(stderr, "World size must be greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int number;
  if (world_rank == 0)
  {
    // If we are rank 0, set the number to -1 and send it to process 1
    number = 23;
    MPI_Isend(&number, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &request);
    sleep(6);
    while (!done)
    {
      printf("test after sleep!\n");
      fflush(stdout);
      MPI_Test(&request, &done, MPI_STATUS_IGNORE);
      printf("exit after test after sleep\n");
      fflush(stdout);
    }
  }
  else if (world_rank == 1)
  {
    sleep(6);
    MPI_Recv(&number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("rank %d received int %d\n", world_rank, number);
    fflush(stdout);
  }
  MPI_Finalize();
}