#include <iostream>
#include "./headers/spimi.h"
using namespace std;

int main(){    
    const int threshold = 1000; //threshold for flushing to disk
    string dataDir = "./corpus/";
    SPIMI spimi(dataDir, threshold);
    spimi.invert();
    spimi.mergeIndexes();
    spimi.saveDocumentsMap();

    cout<< "Inverted Indexing Completed Successfully!" << endl;
    cout<< "enter a query : ";
    string query;
    getline(cin, query);
    spimi.search(query, 4);

    return 0;
}