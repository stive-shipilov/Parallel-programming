#include <stdio.h>
#include <string.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        printf("Программа требует 2 процесса\n");
        MPI_Finalize();
        return 1;
    }
    
    MPI_Win win;
    int *shared_data;
    
    if (rank == 0) {
        // Процесс-сервер
        MPI_Alloc_mem(sizeof(int), MPI_INFO_NULL, &shared_data);
        *shared_data = 100; // Начальное значение
        
        MPI_Win_create(shared_data, sizeof(int), sizeof(int), 
                       MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        
        printf("сервер (rank 0): создал window со значением %d\n", *shared_data);
        
        // Пауза для клиентов
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Локальное обновление
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
        *shared_data += 50;
        MPI_Win_unlock(0, win);
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        printf("Сервер (rank 0): итоговое значение = %d\n", *shared_data);
        
        MPI_Win_free(&win);
        MPI_Free_mem(shared_data);
    } 
    else {
        // Клиенты
        MPI_Win_create(NULL, 0, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Чтение удалённой памяти
        int local_value;
        MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, win);
        MPI_Get(&local_value, 1, MPI_INT, 0, 0, 1, MPI_INT, win);
        MPI_Win_unlock(0, win);
        
        printf("Клиент (rank %d): читает значение %d от сервера\n", rank, local_value);
        
        // Запись в удалённую память
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, win);
        int new_value = rank * 10;
        MPI_Accumulate(&new_value, 1, MPI_INT, 0, 0, 1, MPI_INT, MPI_SUM, win);
        MPI_Win_unlock(0, win);
        
        printf("Клиент (rank %d): добавил значение %d серверу\n", rank, new_value);
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        MPI_Win_free(&win);
    }
    
    MPI_Finalize();
    return 0;
}