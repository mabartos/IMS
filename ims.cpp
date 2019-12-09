#include "simlib/src/simlib.h"

#include <iostream>
#include <stdio.h>
#include <getopt.h>

using namespace std;

#define BLOCK_CAPACITY 2400
#define BLOCK_IDEAL_COUNT 2016
#define HOUR 60 * 60
#define TWO_WEEKS 60 * 60 * 24 * 14
#define YEAR 60 * 60 * 24 * 365

class PowerSource {
public:

    PowerSource() = default;

    double asia_production = .68;
    double europe_production = .17;
    double america_production = .15;

    double asic_power; // Ws
    double asic_hash_power; //TH/s
    double hash_rate; // TH/s

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

    void setHashRate(double value) {
        hash_rate = value;
    }

    void setAsicPower(double value){
        asic_power= value / 3600;
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

    void runExperiment1(){
        asia_production=.20;
        europe_production=.63;
    }

    void runExperiment2(){
        asia_coal=.50;
        asia_renewables=.24;
    }

    void runExperiment3(){
        asia_coal=0;
        america_coal=0;
        europe_coal=0;

        asia_renewables=.74;
        america_renewables=.246;
        europe_renewables=.51;
    }
};

PowerSource powerSource;

// INITIAL VALUES
double difficulty; // avg of TH per block
double averageFootprint; // kg CO2 / Ws
double carbonFootprintPerSecTotal = 0;

unsigned long transactionCount = 0;
unsigned long blockCount = 0;

// kg CO2 / Ws

const double scale = 1.0 / (1000 * HOUR);

const double carbon_footprint_of_coal = scale * 1001;
const double carbon_footprint_of_crude_oil = scale * 840;
const double carbon_footprint_of_renewables = scale * 22.4;
const double carbon_footprint_of_nuclear = scale * 16;
const double carbon_footprint_of_gas = scale * 469;

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
        Activate(Time + 1);
    }
};

class HashRateGenerator : public Process {

    void Behavior() {

    }
};

class HashRateProcess : public Process {

    void Behavior() {
        double changePercent;
        double coeficient;
        while (1) {
            changePercent = Exponential(1.5);
            Wait(Exponential(HOUR * 24));
            coeficient = Random() * changePercent;
            coeficient += Random() <= 0.4 ? -changePercent : 0;
            powerSource.hash_rate += powerSource.hash_rate * coeficient / 100;
            // cout << hashRate << endl;
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
            transactions = BLOCK_CAPACITY - block.Free();
            Leave(block, transactions);
            timeTakenToCreateBlock = difficulty / powerSource.hash_rate;
            Wait(timeTakenToCreateBlock);
            if (blockCount > 0 && blockCount % 2016 == 0) {
                double timeTakenToCreate2016Blocks = Time - timeAt2016BlocksCreated;
                timeAt2016BlocksCreated = Time;
                difficulty *= TWO_WEEKS / timeTakenToCreate2016Blocks;
            }
            blockCount++;
            CalculateConsumption();
        }
    }

    void CalculateConsumption() {
        double consumptionPerSec = (powerSource.hash_rate / powerSource.asic_hash_power) * powerSource.asic_power;
        double consumptionPerBlock = consumptionPerSec * timeTakenToCreateBlock;
        double consumptionPerTransaction = consumptionPerBlock / transactions;

        double carbonFootprintPerSec = consumptionPerSec * averageFootprint;
        double carbonFootprintPerBlock = consumptionPerBlock * averageFootprint;
        double carbonFootprintPerTransaction = consumptionPerTransaction * averageFootprint;

        carbonFootprintPerSecTotal += carbonFootprintPerSec;

        carbonFootprintPerSecond(carbonFootprintPerSec);
        transactionCarbonFootprint(carbonFootprintPerTransaction);
        blockCarbonFootprint(carbonFootprintPerBlock);

    }
};

enum power_source {
    ASIC_POWER,
    ASIC_HASH_POWER,
    HASH_RATE,
    
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
    EUROPE_GAS,

    EXPERIMENT_1,
    EXPERIMENT_2,
    EXPERIMENT_3
};

void assignValueByIndex(int index, double value) {
    if(index==ASIC_POWER){
        powerSource.setAsicPower(value);
        return;
    }else if(index==ASIC_HASH_POWER){
        powerSource.setAsicHashPower(value);
        return;
    } else if (index == HASH_RATE) {
        powerSource.hash_rate = value;
        return;
    }

    value /= 100;
    switch (index) {
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
            {"hash_rate",                 required_argument, 0, 0},

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
            {"experiment1",               no_argument, 0, 0},
            {"experiment2",               no_argument, 0, 0},
            {"experiment3",               no_argument, 0, 0},
            {NULL, 0, NULL,                                     0}
    };

    while ((c = getopt_long(argc, argv, "a:e:", options, &option_index)) != -1) {
        
        if(option_index==EXPERIMENT_1){
            powerSource.runExperiment1();
            return;
        }else if(option_index==EXPERIMENT_2){
            powerSource.runExperiment2();
            return;
        }else if(option_index==EXPERIMENT_3){
            powerSource.runExperiment3();
            return;
        }

        char *error = nullptr;
        double value = strtod(optarg, &error);
        if (option_index != ASIC_POWER && option_index != ASIC_HASH_POWER && option_index != HASH_RATE) {
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

    powerSource.setAsicHashPower(4.2438);
    powerSource.setAsicPower(1535.62);
    powerSource.setHashRate(35036000);

    parseArgs(argc, argv);

    averageFootprint =
        powerSource.getCoalTotal() * carbon_footprint_of_coal +
        powerSource.getCrudeOilTotal() * carbon_footprint_of_crude_oil +
        powerSource.getRenewablesTotal() * carbon_footprint_of_renewables +
        powerSource.getNuclearTotal() * carbon_footprint_of_nuclear +
        powerSource.getGasTotal() * carbon_footprint_of_gas;


    difficulty = powerSource.hash_rate * 512;

    Init(0, YEAR);

    (new TransactionGenerator)->Activate();
    (new BlockProcess)->Activate();
    (new HashRateProcess)->Activate();

    Run();

    carbonFootprintPerSecond.Output();
    transactionCarbonFootprint.Output();
    blockCarbonFootprint.Output();

    double annualCarbonFootprintTotal = (carbonFootprintPerSecTotal / blockCount) * YEAR / 1000000000;

    cout << "Annual carbon footprint: " << annualCarbonFootprintTotal << " Mt CO2" << endl;
    return 0;
}

