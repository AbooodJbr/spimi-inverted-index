#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <queue>
#include <set>
#include <algorithm>
using namespace std;
namespace fs = std::filesystem;

class SPIMI{
private :
int threshold; 
string dataDir;
int indexFileCount;
unordered_set<string> stopWords;
map<int, string> documentsIDs; 
map<string, map<int, unordered_set<int>>> finalIndex;

//helpter functions
// creates or clears a directory
void prepareDirectory(const string& dirPath); 
// writes the in-memory index to a file
void WriteIndexToFile(const map<string, map<int, vector<int>>>& index, int& indexFileCount);
// adds a term and its postings to the final JSON index
void addToJSON(ofstream& outFile, const string& term,
                const map<int, priority_queue<int, vector<int>, greater<int>>>& docMap);
// adds a term from a line to the merged index
string addTermToIndex(const string& line, map<string, map <int, priority_queue<int, vector<int>, greater<int>>>>& mergedIndex);
// intersects two posting lists
vector<int> intersectTwo(vector<int> a, vector<int> b);
// intersects multiple posting lists
vector<int> intersectMultiple(vector<vector<int>> lists);

public:
    SPIMI(string data_directory, int flush_threshold)
        : dataDir(data_directory), threshold(flush_threshold), indexFileCount(0) {
            /*
            Brief: Initialize the indexer with input directory and flush threshold.
            Params:
            - data_directory: Directory containing documents to index.
            - flush_threshold: Max unique terms to hold before flushing to disk.
            Returns: None
            */
            stopWords = {
            "the", "is", "of", "and", "in", "to", "at", "on", "for", "a", "an"
            };
            prepareDirectory("./indexes/");
            prepareDirectory("./final_index/"); 
    };

    void invert(){
        /*
        Brief: Build partial inverted indexes from the corpus using SPIMI.
        Params: None
        Returns: None
        */
        int count = 0; 
        map<string, map<int, vector<int>>> index;
        for(const auto& entry: fs::directory_iterator(dataDir)){
            cout << entry.path() << endl;
            documentsIDs[++count] = entry.path().string();

            ifstream file(entry.path());
            if (!file.is_open()) {
                cout << "Failed to open file: " << entry.path() << endl;
                continue;
            }

            string line;
            int position = 0;

            while (getline(file, line)) {
                string token;
                for (char ch : line) {

                    if (isalnum(ch)) {
                        token += tolower(ch);
                    } else if (!token.empty()) {

                        if (index.size() >= threshold) {
                            WriteIndexToFile(index, indexFileCount);
                            index.clear();
                        }

                        if(token.length() >= 3 && stopWords.count(token) == 0){
                            index[token][count].push_back(position);
                        }

                        position++;
                        token.clear();
                    }
                }
            }
            file.close();
        }

        if (!index.empty()) {
            WriteIndexToFile(index, indexFileCount);
            index.clear();
        }
    };

    void mergeIndexes(){
        /*
        Brief: Merge flushed index files into a single JSON inverted index.
        Params: None
        Returns: None
        */ 
    
        //map to the final index    
        map<string, map <int, priority_queue<int, vector<int>, greater<int>>>> mergedIndex;
        //maping file name to their ofstream objects
        map<string, pair<string, ifstream>> indexFiles;

        for(const auto& entry: fs::directory_iterator("./indexes/")){
            cout << entry.path() << endl;
            ifstream file(entry.path());
            if (!file.is_open()) {
                cerr << "Failed to open file: " << entry.path() << endl;
                continue;
            }
            string line;
            if (getline(file, line) && !line.empty()) {
                string term = addTermToIndex(line, mergedIndex);
                indexFiles[entry.path().string()] = make_pair(term, move(file));
                }
        }

        // write to final index file
        ofstream outFile("./final_index/final_index.json");
        if(!outFile.is_open()){
            cout << "Failed to open output file." << endl;
        }
        outFile << "{\n";

        bool firstTerm = true;
        while(!mergedIndex.empty()){
            auto termIt = mergedIndex.begin();
            const string& term = termIt->first;
            const auto& docMap = termIt->second;

            if (!firstTerm) {
                outFile << ",\n";
            }
            firstTerm = false;

            addToJSON(outFile, term, docMap);
            mergedIndex.erase(termIt); 

            vector<string> files_to_erase;
            for (auto& [filepath, fileData] : indexFiles) {
                auto& [currentTerm, fileStream] = fileData;
                string line;                
                // read next line
                if (!getline(fileStream, line) || fileStream.eof()) {
                    // Handle end of file
                    fileStream.close();
                    files_to_erase.push_back(filepath);
                    continue;
                }

                // add term to index
                if (!line.empty()) {
                    string nextTerm = addTermToIndex(line, mergedIndex);
                    currentTerm = nextTerm;
                }
            }
            // erase finished files from map
            for (const auto& file_path : files_to_erase) {
                indexFiles.erase(file_path);
            }
        }

        outFile << "\n}\n";
        outFile.close();
        // cout << "/n merged index size: " << finalIndex.size() << endl;
};

    void search(const string& query, int windowSize) {

        /*
        Brief: Query the final index for terms appearing within a positional window.
        Params: 
        - query: Raw query string to tokenize and search.
        - windowSize: Max allowed gap between consecutive query terms.
        Returns: None
        */

        cout << "-------------------" << endl;
        cout<< finalIndex.size() << endl;
        cout << "Searching for: " << query << endl;
        // 1. tokenize the query
        vector<string> tokens;
        string currentToken;        
        for(char ch : query) {
            if(isalnum(ch)) {
                currentToken += tolower(ch);
            } else if(!currentToken.empty()) {
                if(currentToken.length() >= 3 && stopWords.count(currentToken) == 0) {
                    tokens.push_back(currentToken);
                }
                currentToken.clear();
            }
        }
        // add last token if exists
        if(!currentToken.empty()) {
            tokens.push_back(currentToken);
        }

        // 2. retrieve posting lists for each token
        vector <vector<int>> allTermsDocs;

        // check if all tokens are in the index and collect their posting lists
        for(const string& term : tokens) {
            if(finalIndex.count(term) > 0) {
                vector<int> termDocs;
                for(const auto& [docID, positions] : finalIndex[term]) {
                    termDocs.push_back(docID);
                }
                allTermsDocs.push_back(termDocs);
            } else {
                cout << "Term not found: " << term << endl;
                break;
            }
        }
        
        // 3. intersect posting lists to find common documents
        vector<int> matchingDocs = intersectMultiple(allTermsDocs);
        // cout << "Matching documents: " << matchingDocs.size() << endl;

        // 4. check for proximity within window size
        // final results doc id -> position
        map<int, set<int>> results;
        for (int docID : matchingDocs) {
            for (auto pos0 : finalIndex[tokens[0]][docID]) {
                int currentPos = pos0;
                bool sequenceMatches = true;

                for (int i = 1; i < tokens.size(); ++i) {
                    bool tokenFound = false;
                    for (auto posNext : finalIndex[tokens[i]][docID]) {
                        if (posNext > currentPos && posNext <= currentPos + windowSize) {
                            currentPos = posNext;
                            tokenFound = true;
                            break;
                        }
                    }
                    if (!tokenFound) {
                        sequenceMatches = false;
                        break;
                    }
                }

                if (sequenceMatches) {
                    // store the **starting position of the phrase**
                    results[docID].insert(pos0);
                }
            }
        }

        // 5. output results
        // cout << "Final Results Count: " << results.size() << endl;
        if(results.empty()) {
            cout << "No documents found matching the query within the specified window size." << endl;
            return;
        }
        for (auto& [docID, positions] : results) {
        cout << "Document ID: " << docID << " Path: " << documentsIDs[docID] << endl;
        cout << "Phrase positions: ";
        for (int pos : positions) {
            cout << pos << " ";
        }
        cout << endl;
}
}


void saveDocumentsMap() {
    /*
    Brief: Persist document ID to path mapping as CSV.
    Params: None
    Returns: None
    */
    ofstream outFile("./final_index/documents_map.csv");
    for (const auto& [docID, path] : documentsIDs) {
        outFile << docID << ", " << path << "\n";
    }
    outFile.close();
}
};


void SPIMI::prepareDirectory(const string& dirPath) {
    /*  
    Brief: Remove existing directory and recreate it empty.
    Params:
    - dirPath: Directory path to prepare.
    Returns: None
    */
    if (fs::exists(dirPath)) {
        fs::remove_all(dirPath);
    } 
    fs::create_directory(dirPath);
}

void SPIMI::WriteIndexToFile(const map<string, map<int, vector<int>>>& index, int& indexFileCount) {

    /* 
    Brief: Flush the in-memory index to a numbered text file.
    Params:
    - index: In-memory index data.
    - indexFileCount: Counter used to generate file names.
    Returns: None
    */

    ofstream outFile( "./indexes/index_" + to_string(++indexFileCount) + ".txt");
    for (const auto& [term, docMap] : index) {    
        outFile << term << " " << docMap.size();
        for (const auto& [docID, positions] : docMap) {
            outFile << " " << docID << ":";
            for (int pos : positions) {
                outFile << pos << ",";
            }
        }
        outFile << "\n";
    }
    outFile.close();
}

void SPIMI::addToJSON(ofstream& outFile, const string& term,
                const map<int, priority_queue<int, vector<int>, greater<int>>>& docMap) {

    /* 
    Brief: Write one term and its postings into the JSON index output.
    Params:
    - outFile: Target JSON stream.
    - term: Term being serialized.
    - docMap: Doc IDs mapped to sorted positions.
    Returns: None
    */

    outFile << "  \"" << term << "\": [ " << docMap.size() << ", {\n";

    bool firstDoc = true;
    for (const auto& [docID, positions] : docMap) {
        if (!firstDoc) outFile << ",\n";
        firstDoc = false;

        outFile << "    \"" << docID << "\": [";
        priority_queue<int, vector<int>, greater<int>> temp = positions;

        bool firstPos = true;
        while (!temp.empty()) {
            if (!firstPos) outFile << ", ";
            firstPos = false;
            outFile << temp.top();
            finalIndex[term][docID].insert(temp.top());
            temp.pop();
        }
        outFile << "]";
    }

    outFile << "\n  }]";
}

string SPIMI::addTermToIndex(const string& line, map<string, map <int, priority_queue<int, vector<int>, greater<int>>>>& mergedIndex) {
    /*
    Brief: Parse one line from a partial index and load it into the merged index.
    Params:
    - line: Text line containing term and postings.
    - mergedIndex: Aggregated index being built.
    Returns:
    - Parsed term string.
    */

    stringstream lineStream(line);
    string term, skip, postings;
    lineStream >> term >> skip; // read the term and skip frequency
    getline(lineStream, postings); // read the rest as postings   
    
    stringstream postingsStream(postings);
    string posting;
    while (postingsStream >> posting) { 
        int colonPos = posting.find(':');
        int docID = stoi(posting.substr(0, colonPos));

        string positionsStr = posting.substr(colonPos + 1);

        stringstream posStream(positionsStr);
        string pos;
        while (getline(posStream, pos, ',')) {
            if (!pos.empty()) { 
                mergedIndex[term][docID].push(stoi(pos));
            }
        }
    }
    return term;
}

vector<int> SPIMI::intersectTwo(vector<int> a, vector<int> b) {
    /* 
    Brief: Return intersection of two posting lists.
    Params:
    - a: First posting list.
    - b: Second posting list.
    Returns:       
    - Vector of doc IDs present in both lists.
    */

    vector<int> result;
    set_intersection(a.begin(), a.end(), b.begin(), b.end(), back_inserter(result));
    return result;
}

vector<int> SPIMI::intersectMultiple(vector<vector<int>> lists) {
    /*
    Brief: Intersect multiple posting lists.
    Params:
    - lists: Collection of posting lists to intersect.
    Returns:
    - Vector of doc IDs present in every list.
    */
    if (lists.empty()) return {};
    vector<int> result = lists[0];
    for (size_t i = 1; i < lists.size(); ++i) {
        result = intersectTwo(result, lists[i]);
    }
    return result;
}