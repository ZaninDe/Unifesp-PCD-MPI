#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define MAX_SIZE 2048
#define MAX_GEN 2000

float **current_grid, **new_grid;

void InitGeneration(int grid_size) {
    int i, j;
    current_grid = (float**) malloc((grid_size + 2) * sizeof(float *));
    new_grid = (float**) malloc((grid_size + 2) * sizeof(float *));

    for(i = 0; i < grid_size + 2; i++) {
        current_grid[i] = (float*) malloc(MAX_SIZE * sizeof(float));
        new_grid[i] = (float*) malloc(MAX_SIZE * sizeof(float));

        for(j = 0; j < MAX_SIZE; j++) {
            current_grid[i][j] = 0.0;
            new_grid[i][j] = 0.0;
        }
    }
}

void FreeGrid(int grid_size) {
    for(int i = 0; i < grid_size + 2; i++) {
        free(new_grid[i]);
        free(current_grid[i]);
    }

    free(new_grid);
    free(current_grid);
}

int TotalLivingCells(int i, int j, int grid_size) {
    int max_x = MAX_SIZE - 1;
    int max_y = grid_size - 1;

    int live_count = 0;
    int x, y, neighbor_x, neighbor_y;

    for(x = i - 1; x <= i + 1; x++) {
        for(y = j - 1; y <= j + 1; y++) {
            if(x == i && y == j) {
            } else {
                neighbor_x = x;
                neighbor_y = y;

                if(x < 0) {
                    neighbor_x = max_x;
                } else if(x > max_x) {
                    neighbor_x = 0;
                }
                
                if (y < 0) {
                    neighbor_y = max_y;
                } else if(y > max_y) {
                    neighbor_y = 0;
                }

                live_count += current_grid[neighbor_x][neighbor_y];
            }
        }
    }

    return live_count;
}

int CellUpdate(int size) {
    float living_cells = 0.0;

    for(int i = 1; i <= size; i++) {
        for(int j = 0; j < MAX_SIZE; j++) {
            if(current_grid[i][j] == 1 && (TotalLivingCells(i, j, size) < 2 || TotalLivingCells(i, j, size) > 3)) {
                new_grid[i][j] = 0.0;
            } else if(current_grid[i][j] == 0 && TotalLivingCells(i, j, size) == 3) {
                new_grid[i][j] = 1.0;
            }
            living_cells += new_grid[i][j];
        }
    }
    return living_cells;
}

void AddInitialCells(float **grid) {
    size_t i = 1, j = 1;
    grid[i][j + 1] = 1.0;
    grid[i + 1][j + 2] = 1.0;
    grid[i + 2][j] = 1.0;
    grid[i + 2][j + 1] = 1.0;
    grid[i + 2][j + 2] = 1.0;

    i = 10; j = 30;
    grid[i][j + 1] = 1.0;
    grid[i][j + 2] = 1.0;
    grid[i + 1][j] = 1.0;
    grid[i + 1][j + 1] = 1.0;
    grid[i + 2][j + 1] = 1.0;
}


void CopyGrid(int size) {
    for(int i = 0; i < size + 2; i++) {
        for(int j = 0; j < MAX_SIZE; j++) {
           current_grid[i][j] = new_grid[i][j];
        }
    }
}

void Update(int previous_rank, int size, int next_rank) {
    MPI_Request request_1, request_2, request_3, request_4;
    MPI_Isend(current_grid[1], MAX_SIZE, MPI_FLOAT, previous_rank, 10, MPI_COMM_WORLD, &request_1);
    MPI_Isend(current_grid[size], MAX_SIZE, MPI_FLOAT, next_rank, 11, MPI_COMM_WORLD, &request_2);
    MPI_Irecv(current_grid[size + 1], MAX_SIZE, MPI_FLOAT, next_rank, 10, MPI_COMM_WORLD, &request_3);
    MPI_Irecv(current_grid[0], MAX_SIZE, MPI_FLOAT, previous_rank, 11, MPI_COMM_WORLD, &request_4);

    MPI_Wait(&request_1, MPI_STATUS_IGNORE);
    MPI_Wait(&request_2, MPI_STATUS_IGNORE);
    MPI_Wait(&request_3, MPI_STATUS_IGNORE);
    MPI_Wait(&request_4, MPI_STATUS_IGNORE);
}

int main(int argc, char **argv) {
    int n_proccess, id_proccess, previous_proccess, next_proccess, i, count = 0, total_living_cells = 0;
    float subsize;
    double start_time, end_time, total_time;

    if(MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        return -1;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &id_proccess);
    MPI_Comm_size(MPI_COMM_WORLD, &n_proccess);

    subsize = MAX_SIZE / n_proccess;
    int rest = MAX_SIZE % n_proccess;

    if(id_proccess == n_proccess - 1) {
        subsize += rest;
    }

    InitGeneration(subsize);

    MPI_Barrier(MPI_COMM_WORLD);

    if(id_proccess == 0) {
        AddInitialCells(current_grid);
        AddInitialCells(new_grid);
    }

    previous_proccess = (id_proccess + n_proccess - 1) % n_proccess;
    next_proccess = (id_proccess + 1) % n_proccess;


    start_time = MPI_Wtime();

    for(i = 0; i < MAX_GEN; i++) {
        Update(previous_proccess, subsize, next_proccess);

        MPI_Barrier(MPI_COMM_WORLD);

        count = CellUpdate(subsize);

        CopyGrid(subsize);

        MPI_Barrier(MPI_COMM_WORLD);

        MPI_Reduce(&count, &total_living_cells, 1, MPI_FLOAT, MPI_SUM, 0, MPI_COMM_WORLD);

        if(id_proccess == 0) {
            printf("Geração %d: %d\n", i + 1, total_living_cells);
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }

    end_time = MPI_Wtime();
    total_time = end_time - start_time;

    if (id_proccess == 0) {
        printf("Tempo de execução: %f segundos\n", total_time);
    }

    FreeGrid(subsize);

    MPI_Finalize();
    return 0;
}
