/*
note: need to start serialoscd first!
assumes port=13808
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "lo/lo.h"

static lo_address monome = lo_address_new(NULL, "13808");
static lo_address pd = lo_address_new(NULL, "9000");

static int grid[8][8];
static int diff[8][8];

int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}

void initialize_random(void)
{
    printf("initializing...\n");
    int r;
    srand(time(NULL));
    for (int i=0; i<8; i++) {
        for (int j=0; j<8; j++) {
            r = rand() % 100;
            if (r<40) {
                grid[i][j] = 1;
                lo_send(monome, "/monome/grid/led/set", "iii", i, j, 1);
            } else {
                grid[i][j] = 0;
                lo_send(monome, "/monome/grid/led/set", "iii", i, j, 0);
            }
        }
    }
}

void initialize_glider(void)
{
    // first zero everything
    for (int i=0; i<8; i++) {
        for (int j=0; j<8; j++) {
            grid[i][j] = 0;
            lo_send(monome, "/monome/grid/led/set", "iii", i, j, 0);
        }
    }

    grid[2][0] = 1;
    lo_send(monome, "/monome/grid/led/set", "iii", 2, 0, 1);
    grid[2][1] = 1;
    lo_send(monome, "/monome/grid/led/set", "iii", 2, 1, 1);
    grid[2][2] = 1;
    lo_send(monome, "/monome/grid/led/set", "iii", 2, 2, 1);
    grid[1][2] = 1;
    lo_send(monome, "/monome/grid/led/set", "iii", 1, 2, 1);
    grid[0][1] = 1;
    lo_send(monome, "/monome/grid/led/set", "iii", 0, 1, 1);
}

int iterate(void)
{
    int live_neighbors;
    int rowind, colind;

    // loop over each grid cell
    for (int i=0; i<8; i++) {
        for (int j=0; j<8; j++) {

            // count neighbors
            live_neighbors = 0;
            for (int drow=-1; drow<2; drow++){
                for (int dcol=-1; dcol<2; dcol++){
                    if (!((drow==0) && (dcol==0))) {
                        rowind = mod(i + drow, 8);
                        colind = mod(j + dcol, 8);
                        live_neighbors += grid[rowind][colind];
                    }
                }
            }

            // assign diff[i][j] based on live_neighbors and current state
            if (grid[i][j]) { // if alive
                if (live_neighbors <= 1) {
                    diff[i][j] = -1; // cell dies from isolation
                    lo_send(pd, "/rule1", "ii", i, j);
                } else if (live_neighbors >= 4) {
                    diff[i][j] = -1; // cell dies from overcrowding
                    lo_send(pd, "/rule2", "ii", i, j);
                } else {
                    diff[i][j] = 0; // cell survives
                }
            } else { // if dead
                if (live_neighbors == 3) {
                    diff[i][j] = 1; // cell is born
                    lo_send(pd, "/rule3", "ii", i, j);
                } else {
                    diff[i][j] = 0; // cell remains dead
                }
            }

        }
    }

    // combine diff with grid, and apply to monome
    int delta;
    int nchanges = 0;
    for (int i=0; i<8; i++) {
        for (int j=0; j<8; j++) {
            delta = diff[i][j];
            grid[i][j] = grid[i][j] + delta;
            if (delta==1) {
                //printf("Cell %d %d is born\n", i, j);
                lo_send(monome, "/monome/grid/led/set", "iii", i, j, 1);
                nchanges++;
            } else if (delta==-1) {
                //printf("Cell %d %d dies\n", i, j);
                lo_send(monome, "/monome/grid/led/set", "iii", i, j, 0);
                nchanges++;
            }
        }
    }

    return nchanges;

}

int keypress_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    if (argv[2]->i == 1){
        int i = argv[0]->i;
        int j = argv[1]->i;
        grid[i][j] = 1;
        lo_send(monome, "/monome/grid/led/set", "iii", i, j, 1);
        lo_send(pd, "/input", "ii", i, j);
    }

    return 0;
}

void error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    int fr, sleepus;

    if (argc<2) {
        printf("Supply frame rate as argument!\n");
    } else {
        fr = atoi(argv[1]);
        sleepus = 1000000/fr;
    }

    lo_server_thread st = lo_server_thread_new("8000", error);
    lo_server_thread_add_method(st, "/monome/grid/key", "iii", keypress_handler, NULL);
    lo_server_thread_start(st);

    initialize_glider();
    //initialize_random();
    usleep(sleepus); // 1/4 second

    int nchanges = 0;
    while(1){
        nchanges = iterate();
        //if (nchanges==0) break;
        usleep(sleepus); // 1/4 second
    }
}
