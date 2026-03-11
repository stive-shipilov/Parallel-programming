#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::cout << "Hello World from rank " << rank << " out of " << size << " processors" << std::endl;

    MPI_Finalize();
    return 0;
}
