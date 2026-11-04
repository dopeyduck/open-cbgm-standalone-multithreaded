/*
 * print_global_stemma.cpp
 *
 *  Created on: Jan 14, 2020
 *      Author: jjmccollum
 */

#ifdef _WIN32
#include <direct.h> //for Windows _mkdir() support
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <unordered_map>
#include <thread>
#include <chrono> 
#include <atomic> 
#include <mutex>  
#include <iomanip> 
#include "cxxopts.hpp"
#include "roaring.hh"
#include "sqlite3.h"
#include "witness.h"
#include "global_stemma.h"


using namespace std;
using namespace roaring;

/**
 * Creates a directory with the given name.
 * The return value will be 0 if successful, and 1 otherwise.
 */
int create_dir(const string & dir) {
	#ifdef _WIN32
		return _mkdir(dir.c_str());
	#else
		umask(0); //this is done to ensure that the newly created directory will have exactly the permissions we specify below
		return mkdir(dir.c_str(), 0755);
	#endif
}


/**
 * Retrieves all rows from the WITNESSES table of the given SQLite database
 * and returns a list of witness IDs populated with its contents.
 * Any witnesses whose IDs are in the set of excluded witness IDs will not be added to the witness list.
 */
list<string> get_list_wit(sqlite3 * input_db, set<string> & excluded_wit_ids) {
	list<string> list_wit = list<string>();
	int rc; //to store SQLite macros
	sqlite3_stmt * select_from_witnesses_stmt;
	sqlite3_prepare(input_db, "SELECT WITNESS FROM WITNESSES ORDER BY ROW_ID", -1, & select_from_witnesses_stmt, 0);
	rc = sqlite3_step(select_from_witnesses_stmt);
	while (rc == SQLITE_ROW) {
		string wit_id = string(reinterpret_cast<const char *>(sqlite3_column_text(select_from_witnesses_stmt, 0)));
		//If the witness's ID is in the excluded set, then skip it:
		if (excluded_wit_ids.find(wit_id) != excluded_wit_ids.end()) {
			rc = sqlite3_step(select_from_witnesses_stmt);
			continue;
		}
		list_wit.push_back(wit_id);
		rc = sqlite3_step(select_from_witnesses_stmt);
	}
	sqlite3_finalize(select_from_witnesses_stmt);
	return list_wit;
}

/**
 * Using rows for the given witness ID from the GENEALOGICAL_COMPARISONS table of the given SQLite database,
 * returns a witness.
 */
/**
 * Using rows for the given witness ID from the GENEALOGICAL_COMPARISONS table of the given SQLite database,
 * returns a witness.
 * Any witnesses whose IDs are in the set of excluded witness IDs will not have their genealogical comparisons added to the witness being populated.
 */
witness get_witness(sqlite3 * input_db, const string & wit_id, set<string> & excluded_wit_ids) {
	int rc; //to store SQLite macros
	//Populate this witness's list of genealogical comparisons to other witnesses:
	list<genealogical_comparison> comps = list<genealogical_comparison>();
	sqlite3_stmt * select_from_genealogical_comparisons_stmt;
	sqlite3_prepare(input_db, "SELECT * FROM GENEALOGICAL_COMPARISONS WHERE PRIMARY_WIT=? ORDER BY ROW_ID", -1, & select_from_genealogical_comparisons_stmt, 0);
	sqlite3_bind_text(select_from_genealogical_comparisons_stmt, 1, wit_id.c_str(), -1, SQLITE_STATIC);
	rc = sqlite3_step(select_from_genealogical_comparisons_stmt);
	while (rc == SQLITE_ROW) {
		genealogical_comparison comp;
		string primary_wit_id = string(reinterpret_cast<const char *>(sqlite3_column_text(select_from_genealogical_comparisons_stmt, 1)));
		comp.primary_wit = primary_wit_id;
		string secondary_wit_id = string(reinterpret_cast<const char *>(sqlite3_column_text(select_from_genealogical_comparisons_stmt, 2)));
		comp.secondary_wit = secondary_wit_id;
		//If the secondary witness's ID is in the excluded set, then skip it:
		if (excluded_wit_ids.find(secondary_wit_id) != excluded_wit_ids.end()) {
			rc = sqlite3_step(select_from_genealogical_comparisons_stmt);
			continue;
		}
		int extant_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 3);
		const char * extant_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 3));
		Roaring extant = Roaring::readSafe(extant_buf, extant_bytes);
		comp.extant = extant;
		int agreements_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 4);
		const char * agreements_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 4));
		Roaring agreements = Roaring::readSafe(agreements_buf, agreements_bytes);
		comp.agreements = agreements;
		int prior_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 5);
		const char * prior_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 5));
		Roaring prior = Roaring::readSafe(prior_buf, prior_bytes);
		comp.prior = prior;
		int posterior_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 6);
		const char * posterior_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 6));
		Roaring posterior = Roaring::readSafe(posterior_buf, posterior_bytes);
		comp.posterior = posterior;
		int norel_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 7);
		const char * norel_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 7));
		Roaring norel = Roaring::readSafe(norel_buf, norel_bytes);
		comp.norel = norel;
		int unclear_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 8);
		const char * unclear_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 8));
		Roaring unclear = Roaring::readSafe(unclear_buf, unclear_bytes);
		comp.unclear = unclear;
		int explained_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 9);
		const char * explained_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 9));
		Roaring explained = Roaring::readSafe(explained_buf, explained_bytes);
		comp.explained = explained;
		float cost = float(sqlite3_column_double(select_from_genealogical_comparisons_stmt, 10));
		comp.cost = cost;
		comps.push_back(comp);
		rc = sqlite3_step(select_from_genealogical_comparisons_stmt);
	}
	sqlite3_finalize(select_from_genealogical_comparisons_stmt);
	witness wit = witness(wit_id, comps);
	return wit;
}

/**
 * Entry point to the script.
 */
int main(int argc, char* argv[]) {
	// Print the start date and time
	auto script_start = std::chrono::system_clock::now();
	time_t script_start_time = std::chrono::system_clock::to_time_t(script_start);
	std::cout << "Script started at: " << std::put_time(std::localtime(&script_start_time), "%Y-%m-%d %H:%M:%S") << std::endl;
	//Read in the command-line options:
	set<string> excluded_wit_ids = set<string>();
	float proportion_extant = 0.0;
	bool print_lengths = false;
	bool flow_strengths = false;
	string input_db_name = string();
	try {
		cxxopts::Options options("print_global_stemma", "Print a global stemma graph to a .dot output files. The output file will be placed in the \"global\" directory.");
		options.custom_help("[-h] [-e wit_1 -e wit_2 ...] [-p proportion] [--lengths] [--strengths] input_db");
		options.positional_help("").show_positional_help();
		options.add_options("")
				("h,help", "print this help")
				("e,excluded", "IDs of witnesses to exclude from the global stemma", cxxopts::value<vector<string>>())
				("p,proportion_extant", "minimum proportion of variation units at which a witness must be extant to be included in the global stemma", cxxopts::value<float>())
				("lengths", "print genealogical costs as edge lengths")
				("strengths", "format edges to reflect flow strengths");
		options.add_options("positional")
				("input_db", "genealogical cache database", cxxopts::value<vector<string>>());
		options.parse_positional({"input_db"});
		auto args = options.parse(argc, argv);
		//Print help documentation and exit if specified:
		if (args.count("help")) {
			cout << options.help({""}) << endl;
			exit(0);
		}
		if (args.count("e")) {
			vector<string> excluded_witnesses = args["e"].as<vector<string>>();
			for (string excluded_wit_id : excluded_witnesses) {
				excluded_wit_ids.insert(excluded_wit_id);
			}
		}
		if (args.count("p")) {
			proportion_extant = args["p"].as<float>();
			//Ensure that the input is between 0 and 1:
			if (proportion_extant < 0.0 || proportion_extant > 1.0) {
				cerr << "Error: The proportion of extant variation units " << proportion_extant << " is not between 0 and 1." << endl;
				exit(1);
			}
		}
		if (args.count("lengths")) {
			print_lengths = args["lengths"].as<bool>();
		}
		if (args.count("strengths")) {
			flow_strengths = args["strengths"].as<bool>();
		}
		//Parse the positional arguments:
		if (!args.count("input_db")) {
			cerr << "Error: 1 positional argument (input_db) is required." << endl;
			exit(1);
		}
		else {
			input_db_name = args["input_db"].as<vector<string>>()[0];
		}
	}
	catch (const cxxopts::OptionException & e) {
		cerr << "Error parsing options: " << e.what() << endl;
		exit(-1);
	}
	//Open the database:
	cout << "Opening database..." << endl;
	sqlite3 * input_db;
	int rc = sqlite3_open(input_db_name.c_str(), & input_db);
	if (rc) {
		cerr << "Error opening database " << input_db_name << ": " << sqlite3_errmsg(input_db) << endl;
		exit(1);
	}
	//If the minimum extant proportion option has been specified, 
	//then count the number of variation units, calculate the minimum number of extant units from this,
	//and add all witnesses below this threshold to the set of excluded witnesses:
	if (proportion_extant > 0.0) {
		cout << "Calculating minimum number of extant variation units..." << endl;
		// Get number of variation units
		int num_variation_units = 0;
		sqlite3_stmt * stmt;
		sqlite3_prepare(input_db, "SELECT COUNT(*) FROM VARIATION_UNITS", -1, &stmt, 0);
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			num_variation_units = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
		int min_extant = (int) ceil(proportion_extant * num_variation_units);
		// Add fragmentary witnesses to exclusion set
		sqlite3_stmt * select_from_genealogical_comparisons_stmt;
		sqlite3_prepare(input_db, "SELECT * FROM GENEALOGICAL_COMPARISONS WHERE PRIMARY_WIT=SECONDARY_WIT ORDER BY ROW_ID", -1, &select_from_genealogical_comparisons_stmt, 0);
		int rc_frag = sqlite3_step(select_from_genealogical_comparisons_stmt);
		while (rc_frag == SQLITE_ROW) {
			string primary_wit_id = string(reinterpret_cast<const char *>(sqlite3_column_text(select_from_genealogical_comparisons_stmt, 1)));
			int extant_bytes = sqlite3_column_bytes(select_from_genealogical_comparisons_stmt, 3);
			const char * extant_buf = reinterpret_cast<const char *>(sqlite3_column_blob(select_from_genealogical_comparisons_stmt, 3));
			Roaring extant = Roaring::readSafe(extant_buf, extant_bytes);
			if (extant.cardinality() < min_extant) {
				excluded_wit_ids.insert(primary_wit_id);
			}
			rc_frag = sqlite3_step(select_from_genealogical_comparisons_stmt);
		}
		sqlite3_finalize(select_from_genealogical_comparisons_stmt);
	}
	cout << "Retrieving witness list..." << endl;
	//Retrieve all witness IDs (for non-excluded witnesses) in the order in which they occur in the table:
	list<string> list_wit = get_list_wit(input_db, excluded_wit_ids);
	cout << "Initializing all witnesses..." << endl;
	//Populate a list of witnesses:
	list<witness> witnesses = list<witness>();
	for (string wit_id : list_wit) {
		//Do not add any witnesses in the excluded set:
		if (excluded_wit_ids.find(wit_id) != excluded_wit_ids.end()) {
			continue;
		}
		witness wit = get_witness(input_db, wit_id, excluded_wit_ids);
		witnesses.push_back(wit);
	}
	// Track unfinished witnesses
	std::set<std::string> unfinished_witnesses;
	for (const witness& wit : witnesses) {
		unfinished_witnesses.insert(wit.get_id());
	}
	//Close the database:
	cout << "Closing database..." << endl;
	sqlite3_close(input_db);
	cout << "Database closed." << endl;
	cout << "Optimizing substemmata (this may take a moment)..." << endl;
	
	// Determine the number of threads to use (limited by CPU cores)
	size_t max_threads = std::thread::hardware_concurrency();
	if (max_threads == 0) {
		max_threads = 4; // Fallback if hardware_concurrency() returns 0
	}
	cout << "Using " << max_threads << " threads (CPU cores available)" << endl;
	
	// Add these before the lambda function:
	std::mutex progress_mutex;
	std::mutex results_mutex;
	size_t total_witnesses = witnesses.size();
	std::atomic<size_t> progress_counter(0);
	std::atomic<size_t> next_witness_index(0);
	
	// Vector to collect results from all threads
	vector<pair<string, list<string>>> all_results;

	// Define a lambda function to optimize the substemmata for a witness
	// Profiling code is added to measure the time taken for each witness's substemmata optimization.
	auto optimize_substemmata = [&]() {
		// Thread-local storage for results
		vector<pair<string, list<string>>> thread_results;
		
		while (true) {
			// Get next witness to process
			size_t current_index = next_witness_index.fetch_add(1);
			if (current_index >= total_witnesses) {
				break; 
			}
			
			// Get reference to the witness at this index
			auto it = witnesses.begin();
			std::advance(it, current_index);
			witness& wit = *it;
			
			// Start timing
			auto start = std::chrono::high_resolution_clock::now();
			list<set_cover_solution> substemmata = wit.get_substemmata();
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = end - start;
			{
				std::lock_guard<std::mutex> lock(progress_mutex);
				auto finished_time = std::chrono::system_clock::now();
				time_t finished_time_t = std::chrono::system_clock::to_time_t(finished_time);
				std::cout << "\nWitness " << wit.get_id() << " substemmata time: " << diff.count() << " s ";
				std::cout << "[" << std::put_time(std::localtime(&finished_time_t), "%Y-%m-%d %H:%M:%S") << "]" << std::endl;
				// Remove processed witness
				unfinished_witnesses.erase(wit.get_id());
				if (unfinished_witnesses.size() == 20) {
					std::cout << "\n20 witnesses left to process:\n";
					for (const auto& id : unfinished_witnesses) {
						std::cout << id << " ";
					}
					std::cout << std::endl;
				}
			}
			if (substemmata.empty()) {
				// Increment progress and print progress bar
				size_t current_progress = ++progress_counter;
				{
					std::lock_guard<std::mutex> lock(progress_mutex);
					int bar_width = 50;
					float progress = float(current_progress) / float(total_witnesses);
					std::cout << "[";
					int pos = bar_width * progress;
					for (int i = 0; i < bar_width; ++i) {
						if (i < pos) std::cout << "=";
						else if (i == pos) std::cout << ">";
						else std::cout << " ";
					}
					std::cout << "] " << int(progress * 100.0) << "% ";
					size_t unfinished = total_witnesses - current_progress;
					std::cout << unfinished << " remaining. ";
					std::cout << "Last finished: " << wit.get_id() << "\r";
					std::cout.flush();
				}
				continue;
			}
			set_cover_solution substemma = substemmata.front();
			list<string> stemmatic_ancestor_ids = list<string>();
			for (set_cover_row row : substemma.rows) {
				stemmatic_ancestor_ids.push_back(row.id);
			}
			// Store result locally instead of modifying witness directly
			thread_results.push_back({wit.get_id(), stemmatic_ancestor_ids});
			// Increment progress and print progress bar after processing is finished
			size_t current_progress = ++progress_counter;
			{
				std::lock_guard<std::mutex> lock(progress_mutex);
				int bar_width = 50;
				float progress = float(current_progress) / float(total_witnesses);
				std::cout << "[";
				int pos = bar_width * progress;
				for (int i = 0; i < bar_width; ++i) {
					if (i < pos) std::cout << "=";
					else if (i == pos) std::cout << ">";
					else std::cout << " ";
				}
				std::cout << "] " << int(progress * 100.0) << "% ";
				size_t unfinished = total_witnesses - current_progress;
				std::cout << unfinished << " remaining. ";
				std::cout << "Last finished: " << wit.get_id() << "\r";
				std::cout.flush();
			}
		}
		
		// Add thread results to global results vector
		{
			std::lock_guard<std::mutex> lock(results_mutex);
			all_results.insert(all_results.end(), thread_results.begin(), thread_results.end());
		}
	};

	// Create a limited number of worker threads
	vector<thread> threads;
	for (size_t i = 0; i < max_threads; ++i) {
		threads.emplace_back(optimize_substemmata);
	}

	// Wait for all threads to finish
	for (thread& t : threads) {
		t.join();
	}
	
	// Apply all results sequentially to avoid thread safety issues
	cout << "Applying substemmata results..." << endl;
	for (const auto& result : all_results) {
		const string& witness_id = result.first;
		const list<string>& stemmatic_ancestor_ids = result.second;
		
		// Find the witness and apply the result
		for (witness& wit : witnesses) {
			if (wit.get_id() == witness_id) {
				wit.set_stemmatic_ancestor_ids(stemmatic_ancestor_ids);
				break;
			}
		}
	}
	cout << "Generating global stemma..." << endl;
	//Construct the global stemma using the witnesses:
	global_stemma gs = global_stemma(witnesses);
	//Create the directory to write the file to:
	string global_dir = "global";
	create_dir(global_dir);
	// Find a unique filename to avoid overwriting
	string base_filename = "global-stemma.dot";
	string filepath = global_dir + "/" + base_filename;
	int suffix = 1;
	while (ifstream(filepath)) {
		filepath = global_dir + "/global-stemma-" + to_string(suffix) + ".dot";
		++suffix;
	}
	// Write to file
	fstream dot_file;
	dot_file.open(filepath, ios::out);
	gs.to_dot(dot_file, print_lengths, flow_strengths);
	dot_file.close();
	cout << "Output written to: " << filepath << endl;
	exit(0);
}
