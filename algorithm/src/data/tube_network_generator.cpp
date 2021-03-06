/* This program is used to simulate and process a graph of tube lines,
 * thereby computing an estimate of the time required to travel from
 * one stop to another.
 *
 * What it DOES factor in:
 *  (+) Time from one station to another (on a train).
 *  (+) An estimate of time spent waiting for trains.
 *  (+) An estimate of time required to change platforms at a station.
 *  (+) An estimate of time required to enter the first station and leave
 *      the last station.
 *
 * What it DOESN'T:
 *  (-) Variable train periods.
 *  (-) Service delays / unexpected stuff happening.
 *  (-) If two tube stations ever have precisely the same name.
 * 
 * This requires three data input files:
 *
 * tube_lines.csv, with schema (line, from, to, weight, rev_weight)
 * where weight and rev_weight are the weight from station from to station to
 * and from station to to station from respectively. (Not always symmetric!)
 *
 * tube_station_overhead.csv, with schema (station, overhead)
 * where overhead is the amount of time required to walk from the platform
 * to the station.
 * (Data accessed on 28 December 2014 from
 *  https://www.whatdotheyknow.com/request/gate_to_platform_and_interchange)
 * 
 * tube_stations.csv, schema (station, OSX, OSY, lat, lng, zone, postcode)
 * where OSX and OSY are the Ordnance Survey coordinates (not used by this
 * program), lat and lng are the latitude and longitude (again, not used by
 * this program), and zone refers to the fare zone. We didn't actually use
 * much of the data, but the information could be useful in the future
 * if users wish to, e.g. avoid Zone 1, or remain in Zones 1 & 2 (due to the
 * relevant Travelcard), etc.
 *
 * This produces one output file, tube_matrix.csv, which contains:
 * - the number of tube stations processed, N
 * - N lines - the name of each tube station, in alphabetical order
 * - N lines - line i contains the time from the ith station to all
 *             other stations (in the same order as above).
 */

#include "../csv_parser.h"
#include "../map_points.h"
#include "../tube_network.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <queue>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::pair;

TubeLine get_line_index(const string& str) {
    // Note that switch-case doesn't work on strings.
    if (str == "Bakerloo") {
        return TubeLine::BAKERLOO;
    }
    if (str == "Central") {
        return TubeLine::CENTRAL;
    }
    if (str == "Circle") {
        return TubeLine::CIRCLE;
    }
    if (str == "District") {
        return TubeLine::DISTRICT;
    }
    if (str == "Hammersmith and City") {
        return TubeLine::HAMMERSMITH_AND_CITY;
    }
    if (str == "Metropolitan") {
        return TubeLine::METROPOLITAN;
    }
    if (str == "Northern") {
        return TubeLine::NORTHERN;
    }
    if (str == "Piccadilly") {
        return TubeLine::PICCADILLY;
    }
    if (str == "Victoria") {
        return TubeLine::VICTORIA;
    }
    if (str == "Waterloo and City") {
        return TubeLine::WATERLOO_AND_CITY;
    }
    // Shouldn't happen.
    return TubeLine::UNDEFINED;
}

// Used to have the priority queue explore nodes in the right order.
struct ReversePairComparator {
    bool operator() (const pair<double, pair<TubeStation*, TubeLine>>& lhs,
                     const pair<double, pair<TubeStation*, TubeLine>>& rhs){
        // Recall that default less for pairs is a lexicographic comparison.
        return lhs > rhs;
    }
};

int main(int argc, char** argv) {
    std::ifstream ifs("tube_stations.csv", std::ifstream::in);
    string str;
    std::getline(ifs, str); // flush header line

    // Constants
    static const int kStationIndex = 0;
    static const int kLatitudeIndex = 3;
    static const int kLongitudeIndex = 4;    

    std::map<string, TubeStation*> tube_stations;

    while (std::getline(ifs, str)) {
        // Read each line, and build the list of nodes.
        vector<string> line_data;
        split(str, ',', &line_data);

        TubeStation *station = new TubeStation(v[0]);

        station->location.lat = stod(line_data[3]);
        station->location.lng = stod(line_data[4]);

        tube_stations.insert(std::make_pair(line_data[0], station));
    }
    std::cout << "Read in " << tube_stations.size() << " candidate stations." 
        << std::endl;

    ifs.close();
    ifs.open("tube_station_overhead.csv", std::ifstream::in);
    std::getline(ifs, str); // flush header line

    while (std::getline(ifs, str)) {
        // Read each line and use it to add the overhead of transitting through
        // each specific station.
        vector<string> v;
        split(str, ',', &v);

        if (tube_stations.find(v[0]) != tube_stations.end()) {
            tube_stations[v[0]]->travel_overhead = std::stod(v[1]);            
        }
    }

    std::map<string, TubeStation*> valid_tube_stations;
    for (const auto& station : tube_stations) {
        // A bit of a hack to clean up the data file, which has some extra tube
        // stations (includes overground and DLR too).
        if (station.second->travel_overhead != -1) {
            valid_tube_stations.insert(station);
        }
    }
    std::cout << "Obtained " << valid_tube_stations.size() << " tube stations."
        << std::endl;

    // Build up the graph.
    ifs.close();
    ifs.open("tube_lines.csv", std::ifstream::in);
    std::getline(ifs, str); // flush header line (again).

    // Constants
    static const int kLineIndex = 0;
    static const int kFromStation = 1;
    static const int kToStation = 2;
    static const int kFromToWeight = 3;
    static const int kToFromWeight = 4;

    int edge_counter = 0;

    while (std::getline(ifs, str)) {
        // Read each line and use it to build up the adjacency lists in the
        // network.
        vector<string> v;
        split(str, ',', &v);

        TubeStation* first_station = valid_tube_stations[v[kFromStation]];
        TubeStation* second_station = valid_tube_stations[v[kToStation]];

        first_station->edge_list.push_back(
            std::make_pair(std::make_pair(second_station, 
                                          get_line_index(v[kLineIndex])),
            std::stod(v[kFromToWeight])));

        second_station->edge_list.push_back(
            std::make_pair(std::make_pair(first_station, 
                                          get_line_index(v[kLineIndex])),
            std::stod(v[kToFromWeight])));

        edge_counter++;
    }
    edge_counter *= 2;
    ifs.close();
    std::cout << "Constructed graph with " << edge_counter << " edges.\n"
        << "Now running simulation..." << std::endl;

    // Build up the network.

    // Constants - we can refine this later if necessary/desired.
    // Models the overhead of waiting for a train.
    // To improve this, we could set it to half the period of the trains
    // on the given line at an average time interval.
    static const double kTrainOverhead = 3.0;

    // Estimate of the maximum amount of time you might save by making a
    // detour (e.g. if you can get from A to B on line I, and from A to B
    // in a longer time but on line J, it's possible that the C, the next
    // stop from B on line J, could be reached more quickly.)
    static const double kMaxDetourEstimate = 20.0;

    // Max number of characters to store timing data to.
    // A little hacky, but because tube stops are generally within 999
    // minutes of each other and our data has a precision of 2 decimal places
    // this is reasonable to save on filesize.
    static const int kTimingPrecision = 6;

    // This is NOT the most efficient algorithm (there are some overlapping
    // subproblems), but we don't expect to run this that often so it seems
    // fine to me. Output as an adjacency matrix (output graph is complete).
    // Output consists of: the number of stations,
    //                     a list of stations alphabetically sorted with
    //                     coordinates (lat, lng),
    //                     followed by the corresponding adjacency matrix.

    // Write the number of tube stations and their names to the output file.
    std::ofstream ofs("tube_matrix.csv", std::ofstream::out);
    ofs << valid_tube_stations.size() << std::endl;

    // Used to preserve accuracy of latitudes and longitudes.
    ofs << std::fixed << std::setprecision(12);
    for (const auto& iter : valid_tube_stations) {
        // Write each tube station's name in order, with its
        // latitude and then longitude.
        // Remember that iterating through a map goes through it in sorted
        // order.
        ofs << iter.second->name << "," << iter.second->location.lat
            << "," << iter.second->location.lng << std::endl;
    }
    int counter = 0;
    for (const auto& iter : valid_tube_stations) {
        // We run a Dijkstra-esque simulation using a priority queue.
        // This looks awkward but makes comparison/ordering of the queue
        // easier.
        std::priority_queue<pair<double, pair<TubeStation*, TubeLine>>,
                            vector<pair<double, 
                                        pair<TubeStation*, TubeLine>>>,
                            ReversePairComparator> queue;

        // Keep track of the best time to every possible station.
        std::map<string, double> best_time;

        // To travel from a station to itself takes no time.
        best_time[iter.second->name] = 0;

        // Initialise the queue with the first station.
        queue.push(std::make_pair(0, 
                        std::make_pair(iter.second, TubeLine::UNDEFINED)));

        while (!queue.empty()) {
            pair<double, pair<TubeStation*, TubeLine>> curr = queue.top();
            queue.pop();

            TubeStation* curr_station = curr.second.first;
            TubeLine curr_line = curr.second.second;
            for (const auto& iter : curr_station->edge_list) {
                TubeStation* target_station = iter.first.first;
                string target_name = target_station->name;

                // Compute the time to reach this destination.
                // We first compute it by adding the edge length, and then
                // further adding overhead of changing, if necessary.
                double dest_time = curr.first + iter.second;
                bool changed_train = (iter.first.second != curr_line &&
                                      curr_line != TubeLine::UNDEFINED);

                if (changed_train) {
                    // Factor in the overhead of changing trains.
                    // Not always perfect, but will do for now (think
                    // Circle-District vs Circle-Piccadilly at SK e.g.)
                    dest_time += kTrainOverhead +
                                 target_station->travel_overhead;
                }

                if (best_time.find(target_name) == best_time.end() ||
                    (best_time.find(target_name) != best_time.end() &&
                     best_time[target_name] > dest_time)) {
                    // So we have done better than the best_time seen so far.
                    best_time[target_name] = dest_time;
                }

                // Heuristic: If you didn't change then there's no way
                // to do better. If you did change then maybe (we consider
                // kMaxDetourEstimate the time we expect you might save).
                if ((changed_train && dest_time - kMaxDetourEstimate 
                        <= best_time[target_name]) ||
                    (!changed_train && dest_time == best_time[target_name])) {
                    // We explore travelling to this station.
                    queue.push(std::make_pair(dest_time,
                        std::make_pair(target_station, iter.first.second)));
                }

            }
        }

        if (best_time.size() != valid_tube_stations.size()) {
            std::cout << "WARNING: Tube graph was found to be unconnected! "
            << std::endl;
        }

        if (++counter % 50 == 0) {
            std::cout << "Simulated " << counter << " stations." << std::endl;
        }

        // Write this row of the Adjacency Matrix to the file.
        // Note that the graph is connected, so the adjacency matrix
        // should be all defined (I hope!)
        string str("");
        for(const auto& inner_iter : best_time) {
            double timing = inner_iter.second;
            if(inner_iter.first != iter.second->name) {
                // We actually need to wait for a train.
                // Add the train waiting constant, plus the overheads
                // of the starting *and* finishing station.
                timing += kTrainOverhead + 
                          valid_tube_stations[inner_iter.first]->
                              travel_overhead +
                          iter.second->travel_overhead;
            }
            str += std::to_string(timing).substr(0, kTimingPrecision);
            str += ",";
        }
        // get rid of the last comma
        str = str.substr(0, str.size() - 1);
        ofs << str << std::endl;
    }
    ofs.close();

    return EXIT_SUCCESS;
}
