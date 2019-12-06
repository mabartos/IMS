#include "simlib/src/simlib.h"

#include <iostream>
#include <stdio.h>

#define BLOCK_CAPACITY 2400
#define BLOCK_IDEAL_COUNT 2016
#define TWO_WEEKS 60 * 60 * 24 * 14

// INITIAL VALUES
double hash_rate = 88092000; // TH/s
double difficulty = hash_rate * 512; // avg of TH per block
double consumption_per_petahash = 56; // Ws
const double transaction_rate = 2;
double average_power_of_asic = 1596; // Wh
double average_hash_power_of_asic = 28; // TH/s
double average_footprint; // kg CO2 / Wh

unsigned long transaction_count = 0;
unsigned long blocks_count = 0;
unsigned long blocks_count_since_last_check = 1;

double last_check = 1;
double time_at_2016_blocks = 2;

double asia = .68;
double europe = .17;
double america = .15;

double coal = asia * .60 + america * .27 + europe * .19;
double crude_oil = asia * .18 + america * .37 + europe * .01;
double renewables = asia * .14 + america * .1 + europe * .32;
double nuclear = asia * .2 + america * .19 + europe * .25;
double gas = asia * .4 + america * .35 + europe * .19;

// kg CO2 / Wh
const double carbon_footprint_of_coal = 900;
const double carbon_footprint_of_crude_oil = 650;
const double carbon_footprint_of_renewables = 10;
const double carbon_footprint_of_nuclear = 5;
const double carbon_footprint_of_gas = 50;

// queue of transactions before being added to a block
Store block("Block", BLOCK_CAPACITY);

class Transaction : public Process {

    void Behavior() {
        Enter(block, Uniform(1, 7));
    }
};

class TransactionGenerator : public Event {

    void Behavior() {
        transaction_count++;
        (new Transaction)->Activate();
        Activate(Time + 1);
    }
};

class DifficultyProcess : public Process {

    void Behavior() {
        while (1) {
            Wait(TWO_WEEKS);
            // every two weeks, check number of crated blocks
            // new difficulty = currentDifficulty * 2 weeks / time spent to create 2016 blocks

            if (blocks_count_since_last_check < BLOCK_IDEAL_COUNT) {
                double timeTaken = TWO_WEEKS / blocks_count_since_last_check * BLOCK_IDEAL_COUNT;
                difficulty *= TWO_WEEKS / timeTaken;
            } else if (blocks_count_since_last_check > BLOCK_IDEAL_COUNT) {
                difficulty *= TWO_WEEKS / (time_at_2016_blocks - last_check);
            }
            // std::cout << "Blocks: " << blocks_count_since_last_check << "\n";
            blocks_count_since_last_check = 0;
            last_check = Time;
        }
    }
};

class HashRateGenerator : public Process {

    void Behavior() {

    }
};

class CarbonFootprintProcess : public Process {

    void Behavior() {
        while (1) {
            Wait(TWO_WEEKS);
            average_footprint =
                coal * carbon_footprint_of_coal +
                crude_oil * carbon_footprint_of_crude_oil +
                renewables * carbon_footprint_of_renewables +
                nuclear * carbon_footprint_of_nuclear +
                gas * carbon_footprint_of_gas;
            std::cout << "average_footprint: " << average_footprint << "\n";
        }
    }
};

class HashRateProcess : public Process {

    void Behavior() {
        double change_percent;
        double coeficient;
        while (1) {
            change_percent = Exponential(1.5);
            Wait(Exponential(3600 * 24));
            coeficient = Random() * change_percent;
            coeficient += Random() <= 0.4 ? -change_percent : 0;
            hash_rate += hash_rate * coeficient / 100;
        }
    }
};

class BlockProcess : public Process {

    unsigned long transactions;
    double time;

    void Behavior() {
        Enter(block, 1);
        while (1) {
            time = Time;
            transactions = BLOCK_CAPACITY - block.Free();
            Leave(block, transactions);
            const double time_to_calculate_block = difficulty / hash_rate;
            Wait(time_to_calculate_block);
            blocks_count_since_last_check++;
            if (blocks_count_since_last_check == 2016) {
                time_at_2016_blocks = Time;
            }
            blocks_count++;
            time = Time - time;
            CalculateConsumption();
        }
    }

    void CalculateConsumption() {
        // std::cout << "count: " << hash_rate / average_hash_power_of_asic << "\npow of asic: " << average_power_of_asic << "\n";
        // printf("(%.2f * %.2f) * %.2f / %ld = %.2f\n", hash_rate / average_hash_power_of_asic, average_power_of_asic / 3600, time, transactions, hash_rate / average_hash_power_of_asic * average_power_of_asic / 3600 * time / transactions);
        double consumption_per_sec = (hash_rate / average_hash_power_of_asic) * average_power_of_asic / 3600;
        double consumption_per_block = consumption_per_sec * time;
        double consumption_per_transaction = consumption_per_block / transactions;
        // std::cout << consumption * time / transactions << "\n";

        double carbon_footprint_per_sec = consumption_per_sec * average_footprint / 3600;
        double carbon_footprint_per_block = consumption_per_block * average_footprint;
        double carbon_footprint_per_transaction = consumption_per_transaction * average_footprint;
        std::cout << carbon_footprint_per_transaction / 3600 << "\n";
        std::cout << consumption_per_transaction << "\n";
    }
};

int main(int argc, char **argv) {

    Init(0, 60 * 60 * 24 * 365);

    (new CarbonFootprintProcess)->Activate();
    (new TransactionGenerator)->Activate();
    (new BlockProcess)->Activate();
    (new DifficultyProcess)->Activate();
    (new HashRateProcess)->Activate();
    Run();

    return 0;
}
