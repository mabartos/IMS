#include "simlib/src/simlib.h"

#include <iostream>
#include <stdio.h>
#include <getopt.h>

using namespace std;

#define BLOCK_CAPACITY 2400
#define BLOCK_IDEAL_COUNT 2016
#define TWO_WEEKS 60 * 60 * 24 * 14
#define YEAR 60 * 60 * 24 * 365

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
double hashRate = 88092000; // TH/s
double difficulty = hashRate * 512; // avg of TH per block
double averageFootprint; // kg CO2 / Ws
double carbonFootprintPerSecTotal = 0;

unsigned long transactionCount = 0;
unsigned long blockCount = 0;

// kg CO2 / Ws

const double milli = .001;

const double carbon_footprint_of_coal = milli * 993 / 3600;
const double carbon_footprint_of_crude_oil = milli * 893 / 3600;
const double carbon_footprint_of_renewables = milli * 642 / 3600;
const double carbon_footprint_of_nuclear = milli * 62 / 3600;
const double carbon_footprint_of_gas = milli * 455 / 3600;

// queue of transactions before being added to a block
Store block("Block", BLOCK_CAPACITY);

// STATISTICS

Stat carbonFootprintPerSecond("Carbon footprint per second");
Stat transactionCarbonFootprint("Carbon footprint of a transaction (kg CO2)");
Stat blockCarbonFootprint("Carbon footprint of a block (kg CO2)");

class Transaction : public Process {

    void Behavior() {
        Enter(block, Uniform(1, 7));
    }
};

class TransactionGenerator : public Event {

    void Behavior() {
        transactionCount++;
        (new Transaction)->Activate();
        Activate(Time + Uniform(0.3, 2.0));
    }
};

class HashRateGenerator : public Process {

    void Behavior() {

    }
};

class CarbonFootprintProcess : public Process {

    void Behavior() {
        while (1) {
            UpdateAverageFootprint();
            Wait(TWO_WEEKS);
        }
    }

    void UpdateAverageFootprint() {
        averageFootprint =
            powerSource.getCoalTotal() * carbon_footprint_of_coal +
            powerSource.getCrudeOilTotal() * carbon_footprint_of_crude_oil +
            powerSource.getRenewablesTotal() * carbon_footprint_of_renewables +
            powerSource.getNuclearTotal() * carbon_footprint_of_nuclear +
            powerSource.getGasTotal() * carbon_footprint_of_gas;
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
            hashRate += hashRate * coeficient / 100;
        }
    }
};

class BlockProcess : public Process {

    unsigned long transactions;
    double timeAt2016BlocksCreated;
    double timeTakenToCreateBlock;

    public: BlockProcess() {
        timeAt2016BlocksCreated = Time;
    }

    void Behavior() {
        Enter(block, 2200);
        while (1) {
            double timeAtTheBeginningOfBlock = Time;
            transactions = BLOCK_CAPACITY - block.Free();
            Leave(block, transactions);
            const double time_to_calculate_block = difficulty / hashRate;
            Wait(time_to_calculate_block);
            if (blockCount > 0 && blockCount % 2016 == 0) {
                double timeTakenToCreate2016Blocks = Time - timeAt2016BlocksCreated;
                timeAt2016BlocksCreated = Time;
                difficulty *= TWO_WEEKS / timeTakenToCreate2016Blocks;
            }
            blockCount++;
            timeTakenToCreateBlock = Time - timeAtTheBeginningOfBlock;
            CalculateConsumption();
        }
    }

    void CalculateConsumption() {
        double consumption_per_sec = (hashRate / powerSource.asic_hash_power) * powerSource.asic_power;
        double consumption_per_block = consumption_per_sec * timeTakenToCreateBlock;
        double consumption_per_transaction = consumption_per_block / transactions;

        double carbon_footprint_per_sec = consumption_per_sec * averageFootprint;
        double carbon_footprint_per_block = consumption_per_block * averageFootprint;
        double carbon_footprint_per_transaction = consumption_per_transaction * averageFootprint;

        carbonFootprintPerSecTotal += carbon_footprint_per_sec;

        carbonFootprintPerSecond(carbon_footprint_per_sec);
        transactionCarbonFootprint(carbon_footprint_per_transaction);
        blockCarbonFootprint(carbon_footprint_per_block);

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

            //ASIA
            {"asia_coal",                 required_argument, 0, 0},
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
    Init(0, YEAR);

    (new CarbonFootprintProcess)->Activate();
    (new TransactionGenerator)->Activate();
    (new BlockProcess)->Activate();
    (new HashRateProcess)->Activate();

    Run();

    carbonFootprintPerSecond.Output();
    transactionCarbonFootprint.Output();
    blockCarbonFootprint.Output();

    double annualCarbonFootprintTotal = (carbonFootprintPerSecTotal / blockCount) * YEAR / 1000000000;
    cout << "Annual carbon footprint: " << annualCarbonFootprintTotal<<endl;
    return 0;
}

