#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

#include "431project.h"

using namespace std;

/*
 * Enter your PSU IDs here to select the appropriate scanning order.
 */
#define PSU_ID_SUM (902589821)

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */


unsigned int currentlyExploringDim = 0;
bool currentDimDone = false;
bool isDSEComplete = false;
int visit2 = 0;
std::string prevConfig;

const int explorationOrder[14] = {
    12, 13, 14,                // BP   (branchsettings, ras, btb)
    11,                        // FPU  (fpwidth)
    0, 1,                      // Core (width, scheduling)
    2, 3, 4, 5, 6, 7, 8, 9     // Cache
};

const int NUM_EXPLORE_DIMS = 14;
bool justStartedDim = true;
/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	string latencySettings;
	//
	//YOUR CODE BEGINS HERE
	//
	//
	// Replace this dumb implementation.
	//latencySettings = "1 1 1";
	//
	//YOUR CODE ENDS HERE
	//
	//return latencySettings;
	// Pad so the size helpers can parse it
    string fullConfig = halfBackedConfig + "0 0 0"; 

    unsigned int dl1size = getdl1size(fullConfig);
    unsigned int il1size = getil1size(fullConfig);
    unsigned int l2size  = getl2size(fullConfig);

    int dl1assoc = extractConfigParam(fullConfig, 4);
    int il1assoc = extractConfigParam(fullConfig, 6);
    int ul2assoc = extractConfigParam(fullConfig, 9);

    int dl1lat = (int)log2((double)dl1size / 2048.0)  + dl1assoc;
    int il1lat = (int)log2((double)il1size / 2048.0)  + il1assoc;
    int ul2lat = (int)log2((double)l2size  / 32768.0) + ul2assoc;

    auto clamp = [](int v){ return v < 0 ? 0 : (v > 9 ? 9 : v); };

    stringstream ss;
    ss << clamp(dl1lat) << " " << clamp(il1lat) << " " << clamp(ul2lat);
    return ss.str();
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {

	// FIXME - YOUR CODE HERE
	//return isNumDimConfiguration(configuration);
	if (!isNumDimConfiguration(configuration)) return 0;

    unsigned int dl1size = getdl1size(configuration);
    unsigned int il1size = getil1size(configuration);
    unsigned int l2size  = getl2size(configuration);

    int l1block = 8  * (1 << extractConfigParam(configuration, 2));
    int l2block = 16 << extractConfigParam(configuration, 8);

    // L1 sizes ∈ [2KB, 64KB]
    if (dl1size < 2048 || dl1size > 65536)  return 0;
    if (il1size < 2048 || il1size > 65536)  return 0;
    // L2 size ∈ [32KB, 1MB]
    if (l2size  < 32768 || l2size > 1048576) return 0;
    // UL2 block ≥ 2× L1 block; max 128B (max enforced by array, but checked anyway)
    if (l2block < 2 * l1block) return 0;
    if (l2block > 128) return 0;
    // Inclusivity: L2 ≥ 2 × (IL1 + DL1)
    if (l2size < 2 * (il1size + dl1size)) return 0;

    return 1;
}

/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations
	std::string nextconfiguration = currentconfiguration;
	// Continue if proposed configuration is invalid or has been seen/checked before.
	while (!validateConfiguration(nextconfiguration) ||
		GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
			return currentconfiguration;
		}

		std::stringstream ss;

        string bestConfig = (optimizeforEXEC == 1) ? bestEXECconfiguration
                                                   : bestEDPconfiguration;
        int curDim = explorationOrder[currentlyExploringDim];

        // Pick next value: start at 0 when entering a new dim, else increment
        int nextValue;
        if (justStartedDim) {
            nextValue = 0;
            justStartedDim = false;
        } else {
            nextValue = extractConfigParam(nextconfiguration, curDim) + 1;
        }
        if (nextValue >= (int)GLOB_dimensioncardinality[curDim]) {
            nextValue = GLOB_dimensioncardinality[curDim] - 1;
            currentDimDone = true;
        }

		for (int d = 0; d < NUM_DIMS - NUM_DIMS_DEPENDENT; ++d) {
            if (d == curDim) ss << nextValue << " ";
            else             ss << extractConfigParam(bestConfig, d) << " ";
        }

        // Append derived latency params
        string configSoFar = ss.str();
        ss << generateCacheLatencyParams(configSoFar);
        nextconfiguration = ss.str();

        if (currentDimDone) {
            currentlyExploringDim++;
            currentDimDone = false;
            justStartedDim = true;
        }
        if (currentlyExploringDim == NUM_EXPLORE_DIMS) {
            isDSEComplete = true;
        }
    }
    return nextconfiguration;
}
/*
		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();
		
		// Make sure we start exploring next dimension in next iteration.
		if (currentDimDone) {
			currentlyExploringDim++;
			currentDimDone = false;
		}
		
		// Signal that DSE is complete after this configuration.
		if (currentlyExploringDim == (NUM_DIMS - NUM_DIMS_DEPENDENT))
			isDSEComplete = true;
	}
	return nextconfiguration;
}
*/