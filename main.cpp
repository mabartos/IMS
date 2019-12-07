#include "simlib/src/simlib.h"

#include <iostream>
#include <stdio.h>
#include <getopt.h>

using namespace std;

#define BLOCK_CAPACITY 2400
#define BLOCK_IDEAL_COUNT 2016
#define TWO_WEEKS 60 * 60 * 24 * 14

class PowerSource {
public:

    PowerSource() = default;

    double asia_production = .68;
    double europe_production = .17;
    double america_production = .15;

    double asic_power = 1596.0 / 3600;
    double asic_hash_power = 28;//TH/s

    double asia_coal = .60;
    double asia_crude_oil = .18;
    double asia_renewables = .14;
    double asia_nuclear = .2;
    double asia_gas = .4;

    double america_coal = .131;
    double america_crude_oil = .365;
    double america_renewables = .115;
    double america_nuclear = .86;
    double america_gas = .306;

    double europe_coal = .19;
    double europe_crude_oil = .01;
    double europe_renewables = .32;
    double europe_nuclear = .25;
    double europe_gas = .19;

    void setAsicPower(double value){
        asic_power=value/3600;
    }

    void setAsicHashPower(double value){
        asic_hash_power=value;
    }

    double getCoalTotal() {
        return asia_production * asia_coal + america_production * america_coal + europe_production * europe_coal;
    }

    double getCrudeOilTotal() {
        return asia_production * asia_crude_oil + america_production * america_crude_oil +
               europe_production * europe_crude_oil;
    }

    double getRenewablesTotal() {
        return asia_production * asia_renewables + america_production * america_renewables +
               europe_production * europe_renewables;
    }

    double getNuclearTotal() {
        return asia_production * asia_nuclear + america_production * america_nuclear +
               europe_production * europe_nuclear;
    }

    double getGasTotal() {
        return asia_production * asia_gas + america_production * america_gas + europe_production * europe_gas;
    }
};

PowerSource powerSource;

// INITIAL VALUES
double hash_rate = 88092000; // TH/s
double difficulty = hash_rate * 512; // avg of TH per block
double consumption_per_petahash = 56; // Ws
const double transaction_rate = 2;
double average_footprint; // kg CO2 / Ws

unsigned long transaction_count = 0;
unsigned long blocks_count = 0;
unsigned long blocks_count_since_last_check = 1;

double last_check = 1;
double time_at_2016_blocks = 2;

// kg CO2 / Ws

const double milli = .001;

const double carbon_footprint_of_coal = milli * 993 / 3600;
const double carbon_footprint_of_crude_oil = milli * 893 / 3600;
const double carbon_footprint_of_renewables = milli * 642 / 3600;
const double carbon_footprint_of_nuclear = milli * 62 / 3600;
const double carbon_footprint_of_gas = milli * 455 / 3600;

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
            // cout << "Blocks: " << blocks_count_since_last_check << endl;
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
                    powerSource.getCoalTotal() * carbon_footprint_of_coal +
                    powerSource.getCrudeOilTotal() * carbon_footprint_of_crude_oil +
                    powerSource.getRenewablesTotal() * carbon_footprint_of_renewables +
                    powerSource.getNuclearTotal() * carbon_footprint_of_nuclear +
                    powerSource.getGasTotal() * carbon_footprint_of_gas;
            //cout << "average_footprint: " << average_footprint << endl;
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
        // cout << "count: " << hash_rate / average_hash_power_of_asic << "\npow of asic: " << average_power_of_asic << endl;
        // printf("(%.2f * %.2f) * %.2f / %ld = %.2f\n", hash_rate / average_hash_power_of_asic, average_power_of_asic / 3600, time, transactions, hash_rate / average_hash_power_of_asic * average_power_of_asic / 3600 * time / transactions);
        double consumption_per_sec = (hash_rate / powerSource.asic_hash_power) * powerSource.asic_power;
        double consumption_per_block = consumption_per_sec * time;
        double consumption_per_transaction = consumption_per_block / transactions;
        // cout << consumption_per_transaction << endl;

        double carbon_footprint_per_sec = consumption_per_sec * average_footprint;
        double carbon_footprint_per_block = consumption_per_block * average_footprint;
        double carbon_footprint_per_transaction = consumption_per_transaction * average_footprint;

        cout << "Carbon Footprint per Sec (kg): " << carbon_footprint_per_sec << endl;
        cout << "Carbon Footprint per Transaction (kg): " << carbon_footprint_per_transaction << endl;
        cout << "Carbon Footprint per Block (kg): " << carbon_footprint_per_block << endl;
    }
};


enum power_source {
    ASIC_POWER,
    ASIC_HASH_POWER,
    ASIA_ENERGY_PRODUCTION,
    AMERICA_ENERGY_PRODUCTION,
    EUROPE_ENERGY_PRODUCTION,

    ASIA_COAL,
    ASIA_CRUDE_OIL,
    ASIA_RENEWABLES,
    ASIA_NUCLEAR,
    ASIA_GAS,

    AMERICA_COAL,
    AMERICA_CRUDE_OIL,
    AMERICA_RENEWABLES,
    AMERICA_NUCLEAR,
    AMERICA_GAS,

    EUROPE_COAL,
    EUROPE_CRUDE_OIL,
    EUROPE_RENEWABLES,
    EUROPE_NUCLEAR,
    EUROPE_GAS
};

void assignValueByIndex(int index, double value) {
    if(index==ASIC_POWER){
        powerSource.setAsicPower(value);
        return;
    }else if(index==ASIC_HASH_POWER){
        powerSource.setAsicHashPower(value);
        return;
    }

    value /= 100;
    switch (index) {
        case ASIA_ENERGY_PRODUCTION:
            powerSource.asia_production = value;
            break;
        case AMERICA_ENERGY_PRODUCTION:
            powerSource.america_production = value;
            break;
        case EUROPE_ENERGY_PRODUCTION:
            powerSource.europe_production = value;
            break;

        case ASIA_COAL:
            powerSource.asia_coal = value;
            break;
        case ASIA_CRUDE_OIL:
            powerSource.asia_crude_oil = value;
            break;
        case ASIA_RENEWABLES:
            powerSource.asia_renewables = value;
            break;
        case ASIA_NUCLEAR:
            powerSource.asia_nuclear = value;
            break;
        case ASIA_GAS:
            powerSource.asia_gas = value;
            break;

        case AMERICA_COAL:
            powerSource.america_coal = value;
            break;
        case AMERICA_CRUDE_OIL:
            powerSource.america_crude_oil = value;
            break;
        case AMERICA_RENEWABLES:
            powerSource.america_renewables = value;
            break;
        case AMERICA_NUCLEAR:
            powerSource.america_nuclear = value;
            break;
        case AMERICA_GAS:
            powerSource.america_gas = value;
            break;

        case EUROPE_COAL:
            powerSource.europe_coal = value;
            break;
        case EUROPE_CRUDE_OIL:
            powerSource.europe_crude_oil = value;
            break;
        case EUROPE_RENEWABLES:
            powerSource.europe_renewables = value;
            break;
        case EUROPE_NUCLEAR:
            powerSource.europe_nuclear = value;
            break;
        case EUROPE_GAS:
            powerSource.europe_gas = value;
            break;
        default:
            break;
    }
}

void parseArgs(int argc, char **argv) {
    int c;
    int option_index = 0;

    static struct option options[] = {
            //ASIC PARAMETERS
            {"asic_power",                required_argument, 0, 0},
            {"asic_hash_power",           required_argument, 0, 0},

            //PRODUCTION OF ELECTRICITY
            {"asia_energy_production",    required_argument, 0, 0},
            {"america_energy_production", required_argument, 0, 0},
            {"europe_energy_production",  required_argument, 0, 0},

            //ASIA
            {"asia_coal",                 required_argument, nullptr, 0},
            {"asia_crude_oil",            required_argument, 0, 0},
            {"asia_renewables",           required_argument, 0, 0},
            {"asia_nuclear",              required_argument, 0, 0},
            {"asia_gas",                  required_argument, 0, 0},

            //AMERICA
            {"america_coal",              required_argument, 0, 0},
            {"america_crude_oil",         required_argument, 0, 0},
            {"america_renewables",        required_argument, 0, 0},
            {"america_nuclear",           required_argument, 0, 0},
            {"america_gas",               required_argument, 0, 0},

            //EUROPE
            {"europe_coal",               required_argument, 0, 0},
            {"europe_crude_oil",          required_argument, 0, 0},
            {"europe_renwables",          required_argument, 0, 0},
            {"europe_nuclear",            required_argument, 0, 0},
            {"europe_gas",                required_argument, 0, 0},
            {NULL, 0, NULL,                                     0}
    };
    
    while ((c = getopt_long(argc, argv, "a:e:", options, &option_index)) != -1) {
        char *error = nullptr;
        double value = strtod(optarg, &error);
        cout<<value<<endl;
        if (option_index != ASIC_POWER && option_index != ASIC_HASH_POWER) {
            if (value < 0 || value > 100) {
                fprintf(stderr, "Value can be in range (0,100)%%\n");
                exit(EXIT_FAILURE);
            }
        }

        if (c == 0) {
            assignValueByIndex(option_index, value);
        }
    }
}

int main(int argc, char **argv) {
    parseArgs(argc, argv);
    Init(0, 60 * 60 * 24 * 365);

    (new CarbonFootprintProcess)->Activate();
    (new TransactionGenerator)->Activate();
    (new BlockProcess)->Activate();
    (new DifficultyProcess)->Activate();
    (new HashRateProcess)->Activate();
    Run();

    return 0;
}
