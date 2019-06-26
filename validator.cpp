#include <cmath>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>

using namespace std;

struct Create {
  uint64_t time;
  int duration;
};

struct Done {
  uint64_t time;
};

void buildMaps(const std::string &fileName, unordered_map<int, Create> &createMap, unordered_map<int, Done> &doneMap) {
  ifstream inFile(fileName);
  if (!inFile) {
    throw runtime_error("Couldnt open file \""+fileName+"\"");
  }

  // [1561585661966] create 58 3337
  regex createRegex(R"Delim(\[([0-9]+)\] create ([0-9]+) ([0-9]+))Delim");
  // [1561585661053] done 49
  regex doneRegex(R"Delim(\[([0-9]+)\] done ([0-9]+))Delim");

  string line;
  while (getline(inFile, line)) {
    smatch matchResult;
    if (regex_match(line, matchResult, createRegex)) {
      int id = stoi(matchResult[2].str());
      Create create;
      create.time = stoll(matchResult[1].str());
      create.duration = stoi(matchResult[3].str());
      if (createMap.find(id) != createMap.end()) {
        throw runtime_error("Creation already happened for id "+to_string(id));
      }
      createMap.insert(make_pair(id, create));
    } else if (regex_match(line, matchResult, doneRegex)) {
      int id = stoi(matchResult[2].str());
      Done done;
      done.time = stoll(matchResult[1].str());
      if (doneMap.find(id) != doneMap.end()) {
        throw runtime_error("Done already happened for id "+to_string(id));
      }
      doneMap.insert(make_pair(id, done));
    } else {
      cout << "No match \"" << line << "\"\n";
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " <sim output file>\n";
    return 0;
  }

  const string kFileName = argv[1];
  unordered_map<int, Create> createMap;
  unordered_map<int, Done> doneMap;
  buildMaps(kFileName, createMap, doneMap);

  unordered_map<int,int> offCounts;

  for (const auto &idDonePair : doneMap) {
    const int id = idDonePair.first;
    const auto createIt = createMap.find(id);
    if (createIt == createMap.end()) {
      cout << "A timer(" << id << ") finished but was never started\n";
    }
    const Create &create = createIt->second;
    const Done &done = idDonePair.second;

    uint64_t actualTimeElapsed = done.time - create.time;
    uint64_t expectedTimeElapsed = create.duration;
    int diff = std::abs(static_cast<int>(actualTimeElapsed-expectedTimeElapsed));
    if (offCounts.find(diff) == offCounts.end()) {
      offCounts.emplace(diff, 0);
    }
    ++offCounts.at(diff);
  }

  for (auto &i : offCounts) {
    cout << "Off by " << i.first << " " << i.second << " time(s)\n";
  }
  return 0;
}