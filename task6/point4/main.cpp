#include <stdio.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, "ranks.bin", 
                  MPI_MODE_CREATE | MPI_MODE_WRONLY,
                  MPI_INFO_NULL, &file);
    
    // Каждый процесс пишет свой ранг
    MPI_Offset offset = rank * sizeof(int);
    MPI_File_write_at(file, offset, &rank, 1, MPI_INT, MPI_STATUS_IGNORE);
    
    MPI_File_close(&file);
    
    // Только процесс 0 читает результат
    if (rank == 0) {
        int ranks[size];
        FILE *f = fopen("ranks.bin", "rb");
        fread(ranks, sizeof(int), size, f);
        fclose(f);
        
        printf("Ranks in order: ");
        for (int i = 0; i < size; i++)
            printf("%d ", ranks[i]);
        printf("\n");
    }
    
    MPI_Finalize();
    return 0;
}