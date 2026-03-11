#include <mpi.h>
#include <iostream>
#include <cmath>
#include <iomanip>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int N = 0;
    N = std::stoi(argv[1]);

    int block_size = (N + size - 1) / size; 

    long double local_sum = 0.0;
    int i = rank * block_size + 1;
    for (; i <= (rank + 1) * block_size && i <= N; ++i) {
        local_sum += 1.0L / i;
    }

    long double total_sum = 0.0;
    MPI_Reduce(&local_sum, &total_sum, 1, MPI_LONG_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Total sum \t" << total_sum << std::endl;
    }

    MPI_Finalize();
    return 0;
}
